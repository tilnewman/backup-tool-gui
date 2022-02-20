#ifndef BACKUP_TASK_QUEUE_HPP_INCLUDED
#define BACKUP_TASK_QUEUE_HPP_INCLUDED
//
// task-queue.hpp
//
#include "task-resources.hpp"

#include <cassert>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace backup
{

    struct TaskQueueStatus
    {
        bool isReady() const noexcept
        {
            return ((queue_size > 0) && (resource_busy_count < resource_count));
        }

        bool isDone() const noexcept { return ((0 == queue_size) && (0 == resource_busy_count)); }

        std::size_t queue_size          = 0;
        std::size_t resource_count      = 0;
        std::size_t resource_busy_count = 0;
        std::size_t completed_count     = 0;
        Progress_t progress_sum         = 0;
    };

    constexpr bool operator==(const TaskQueueStatus & left, const TaskQueueStatus & right)
    {
        return (
            (left.queue_size == right.queue_size) &&
            (left.resource_count == right.resource_count) &&
            (left.resource_busy_count == right.resource_busy_count) &&
            (left.completed_count == right.completed_count) &&
            (left.progress_sum == right.progress_sum));
    }

    constexpr bool operator!=(const TaskQueueStatus & left, const TaskQueueStatus & right)
    {
        return !(left == right);
    }

    //

    // This class maintains a queue of filesystem "Task"s that are waiting to be executed by
    // multiple threads.  A thread cannot execute an enqueued task without it's own dedicated
    // Resource_t.  So this class also maintains a limited number of these Resource_ts in
    // m_resourceCache.  This means the number of tasks that can be run in parallel is limited by
    // the number of cached Resource_ts, or put another way, m_resourceCache.size().
    //
    // Simply spawn as many threads as you want, and have each loop that calls popAndExecute()
    // repeatedly.  Each thread running popAndExecute():
    //  - will execute their taskFunction without the mutex locked, and are free to change their
    // Resource_t without the need to lock anything. and are free to call push() to enqueue
    // more Tasks.
    //
    // If true is returned, that means a Task was taken off of the queue and executed.  If false is
    // retruned, that means either the queue was empty or there were no available Rescource_ts in
    // the cache, and that calling thread will either have to wait until a Task is added to the
    // queue or until another thread finishes and frees up one of the Rescource_ts.
    //
    template <typename Resource_t>
    class ResourceLimitedParallelTaskQueue
    {
      public:
        using resource_t = Resource_t;

        explicit ResourceLimitedParallelTaskQueue(const std::size_t resourceCount)
            : m_mutex()
            , m_completedCount(0)
            , m_queue()
            , m_resourceCache(resourceCount) // this must be the only time this vector reallocates!
        {
            // m_queue.reserve(reserveCount);//TODO put this back after testing
        }

        std::size_t completedCount() const { return m_completedCount; }
        std::size_t resourceCount() const { return m_resourceCache.size(); }
        std::size_t queueLength() const { return m_queue.size(); }
        std::size_t queueMaxCapacity() const { return m_queue.capacity(); }

        TaskQueueStatus push(const EntryConstRefDPair_t & entryDPair)
        {
            std::scoped_lock scopedLock(m_mutex);
            m_queue.push_back({ entryDPair.src, entryDPair.dst });
            return status_WithoutLock();
        }

        template <typename TaskExecuteFunction_t>
        bool popAndExecute(TaskExecuteFunction_t taskExecute)
        {
            ScopedTaskResource<Resource_t> scopedResource{ pop() };

            if (!scopedResource.is_valid)
            {
                return false;
            }

            scopedResource.resource.setup();
            taskExecute(scopedResource.resource);
            ++m_completedCount;
            scopedResource.resource.teardown();
            return true;
        }

        TaskQueueStatus status() const
        {
            std::scoped_lock scopedLock(m_mutex);
            return status_WithoutLock();
        }

      private:
        ScopedTaskResource<Resource_t> pop()
        {
            std::scoped_lock scopedLock(m_mutex);

            std::size_t i{ 0 };
            const std::size_t resourceCount{ m_resourceCache.size() };
            for (; i < resourceCount; ++i)
            {
                if (m_resourceCache[i].isAvailable())
                {
                    break;
                }
            }

            if ((i < resourceCount) && !m_queue.empty())
            {
                Resource_t & resource{ m_resourceCache[i] };
                resource.entry_dpair = std::move(m_queue.back());
                m_queue.pop_back();

                return ScopedTaskResource<Resource_t>(true, m_mutex, resource);
            }
            else
            {
                return ScopedTaskResource<Resource_t>(false, m_mutex, m_resourceCache.back());
            }
        }

        TaskQueueStatus status_WithoutLock() const
        {
            std::size_t busyCount{ 0 };
            Progress_t progressCountTotal{ 0 };

            for (auto & resource : m_resourceCache)
            {
                if (!resource.isAvailable())
                {
                    ++busyCount;
                    progressCountTotal += resource.progress;
                }
            }

            assert(busyCount <= m_resourceCache.size());

            return { m_queue.size(),
                     m_resourceCache.size(),
                     busyCount,
                     m_completedCount,
                     progressCountTotal };
        }

      private:
        mutable std::mutex m_mutex;
        std::size_t m_completedCount;
        std::vector<EntryDPair_t> m_queue;
        std::vector<Resource_t> m_resourceCache;
    };

} // namespace backup

#endif // BACKUP_TASK_QUEUE_HPP_INCLUDED
