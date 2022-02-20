#ifndef BACKUP_TASKER_HPP_INCLUDED
#define BACKUP_TASKER_HPP_INCLUDED
//
// tasker.hpp
//
#include "task-queue.hpp"
#include "task-resources.hpp"
#include "thread-pool.hpp"

#include <future>

namespace backup
{

    class DirectoryCompareTasker;
    class FileCompareTasker;

    struct IBackupContext
    {
        virtual ~IBackupContext() = default;

        virtual void notifyAll()               = 0;
        virtual bool willAbort()               = 0;
        virtual void printStatusUpdateIfTime() = 0;

        virtual FileCompareTasker & fileCompareTasker()           = 0;
        virtual DirectoryCompareTasker & directoryCompareTasker() = 0;

        virtual bool executeTaskCopy(CopyTaskResources &)                   = 0;
        virtual bool executeTaskRemove(RemoveTaskResources &)               = 0;
        virtual bool executeTaskFileCompare(FileCompareTaskResources &)     = 0;
        virtual bool executeTaskDirCompare(DirectoryCompareTaskResources &) = 0;
    };

    //

    template <typename TaskResource_t>
    class ParallelTasker
    {
      public:
        using TaskQueue_t = ResourceLimitedParallelTaskQueue<TaskResource_t>;

        explicit ParallelTasker(IBackupContext & backupContext, const std::size_t parallelCount)
            : m_context(backupContext)
            , m_isFinished(false)
            , m_threadPool()
            , m_taskQueue(parallelCount)
            , m_condVar()
        {}

        virtual ~ParallelTasker() = default;

        bool isFinished() const { return m_isFinished; }
        TaskQueueStatus status() const { return m_taskQueue.status(); }

        void notifyOne() { m_condVar.notify_one(); }
        void notifyAll() { m_condVar.notify_all(); }

        virtual void enqueue(const EntryConstRefDPair_t & entryDPair)
        {
            // after pushing a new task on the queue, check if a thread needs to wake and execute it
            if (m_taskQueue.push(entryDPair).isReady())
            {
                notifyOne();
            }
        }

        void start()
        {
            m_isFinished = false;

            for (std::size_t i{ 0 }; i < m_taskQueue.resourceCount(); ++i)
            {
                m_threadPool.add(std::async(
                    std::launch::async, &ParallelTasker<TaskResource_t>::executeLoop, this));
            }
        }

        void waitUntilFinished()
        {
            notifyAll();

            auto printStatusIfTime = [&]() { m_context.printStatusUpdateIfTime(); };
            m_threadPool.waitUntilAllJoinedAndDestroyed(printStatusIfTime);

            m_isFinished = true;

            m_context.notifyAll();
        }

      private:
        bool executeLoop()
        {
            while (!m_context.willAbort())
            {
                if (executeTask(m_taskQueue))
                {
                    continue;
                }

                if (isDoneAndAllowedToFinish())
                {
                    break;
                }

                std::unique_lock lock(m_condVarMutex);

                m_condVar.wait_for(
                    lock, std::chrono::milliseconds(250), [&]() { return isAllowedToWake(); });
            }

            return true;
        }

        bool isDoneAndAllowedToFinish() const
        {
            const TaskQueueStatus myStatus{ status() };

            if (myStatus.isDone())
            {
                return isAllowedToFinish(myStatus);
            }
            else
            {
                return false;
            }
        }

        virtual bool executeTask(TaskQueue_t & myTaskQueue) = 0;
        virtual bool isAllowedToWake() const { return true; }
        virtual bool isAllowedToFinish(const TaskQueueStatus & myStatus) const = 0;

      protected:
        IBackupContext & m_context;

      private:
        bool m_isFinished;
        ThreadPool m_threadPool;
        TaskQueue_t m_taskQueue;
        std::condition_variable m_condVar;

        static inline std::mutex m_condVarMutex;
    };

    //

    class DirectoryCompareTasker : public ParallelTasker<DirectoryCompareTaskResources>
    {
      public:
        explicit DirectoryCompareTasker(
            IBackupContext & backupContext, const std::size_t parallelCount)
            : ParallelTasker<DirectoryCompareTaskResources>(backupContext, parallelCount)
        {}

        virtual ~DirectoryCompareTasker() = default;

      private:
        bool executeTask(TaskQueue_t & myTaskQueue) override
        {
            auto compareDirs = [&](auto & res) { return m_context.executeTaskDirCompare(res); };
            return myTaskQueue.popAndExecute(compareDirs);
        }

        // not allowed to wake if the number of queued file or dir compare tasks are getting out of
        // hand which helps keep the queue sizes from getting out of hand and using too much memory
        bool isAllowedToWake() const override;

        // there should always be at least one dir compare task, the first/initial task
        // if there are any other dirs to compare, then it could'nt finish without add more
        // so the dir compare tasker can simply wait for its own completed_count to to be >0, and
        // for the queue status to be done which is already true if this functions is executing
        bool isAllowedToFinish(const TaskQueueStatus & myStatus) const override
        {
            return (myStatus.completed_count > 0);
        }
    };

    //

    class FileCompareTasker : public ParallelTasker<FileCompareTaskResources>
    {
      public:
        explicit FileCompareTasker(IBackupContext & backupContext, const std::size_t parallelCount)
            : ParallelTasker<FileCompareTaskResources>(backupContext, parallelCount)
        {}

        virtual ~FileCompareTasker() = default;

      private:
        bool executeTask(TaskQueue_t & myTaskQueue) override
        {
            auto compareFiles = [&](auto & res) { return m_context.executeTaskFileCompare(res); };
            return myTaskQueue.popAndExecute(compareFiles);
        }

        bool isAllowedToWake() const override
        {
            const TaskQueueStatus myStatus{ status() };

            const bool isReadyToExecuteTask{ myStatus.isReady() };
            const bool isFinished{ (myStatus.isDone() && isAllowedToFinish(myStatus)) };

            return (isReadyToExecuteTask || isFinished);
        }

        // not allowed to finish until all the directories have been compared
        bool isAllowedToFinish(const TaskQueueStatus &) const override
        {
            return m_context.directoryCompareTasker().isFinished();
        }
    };

    //

    class CopyTasker : public ParallelTasker<CopyTaskResources>
    {
      public:
        explicit CopyTasker(IBackupContext & backupContext, const std::size_t parallelCount)
            : ParallelTasker<CopyTaskResources>(backupContext, parallelCount)
        {}

        virtual ~CopyTasker() = default;

      private:
        bool executeTask(TaskQueue_t & myTaskQueue) override
        {
            auto copier = [&](auto & res) { return m_context.executeTaskCopy(res); };
            return myTaskQueue.popAndExecute(copier);
        }

        // not allowed to finish until all the threads that might add to our queue have finished
        bool isAllowedToFinish(const TaskQueueStatus &) const override
        {
            return (
                m_context.directoryCompareTasker().isFinished() &&
                m_context.fileCompareTasker().isFinished());
        }
    };

    //

    class RemoveTasker : public ParallelTasker<RemoveTaskResources>
    {
      public:
        explicit RemoveTasker(IBackupContext & backupContext, const std::size_t parallelCount)
            : ParallelTasker<RemoveTaskResources>(backupContext, parallelCount)
        {}

        virtual ~RemoveTasker() = default;

      private:
        bool executeTask(TaskQueue_t & myTaskQueue) override
        {
            auto remover = [&](auto & resource) { return m_context.executeTaskRemove(resource); };
            return myTaskQueue.popAndExecute(remover);
        }

        // not allowed to finish until all the threads that might add to our queue have finished
        bool isAllowedToFinish(const TaskQueueStatus &) const override
        {
            return (
                m_context.directoryCompareTasker().isFinished() &&
                m_context.fileCompareTasker().isFinished());
        }
    };

} // namespace backup

#endif // BACKUP_TASKER_HPP_INCLUDED
