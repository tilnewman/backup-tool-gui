#ifndef BACKUP_TOOL_HPP_INCLUDED
#define BACKUP_TOOL_HPP_INCLUDED
//
// backup-tool.hpp
//
#include "base-file-operations.hpp"
#include "tasker.hpp"

namespace backup
{

    class BackupTool
        : public BaseFileOperations
        , public IBackupContext
    {
      public:
        BackupTool(const std::vector<std::string> & args);
        virtual ~BackupTool() = default;

        void run();

      private:
        void printFinalResults(const bool didAbortEarly);
        void startAndWaitForAllThreadsToFinish();

        void scheduleFileCompare(const EntryConstRefDPair_t & entryDPair) override;
        void scheduleDirectoryCompare(const EntryConstRefDPair_t & entryDPair) override;
        void scheduleFileCopy(const EntryConstRefDPair_t & entryDPair) override;
        void scheduleFileRemove(const EntryConstRefDPair_t & entryDPair) override;

        // all functions below are IBackupContext interface functions
        void notifyAll() override;

        bool willAbort() override { return haveAnyExceptionsBeenThrown(); }

        FileCompareTasker & fileCompareTasker() override { return m_fileCompareTasker; }
        DirectoryCompareTasker & directoryCompareTasker() override { return m_dirCompareTasker; }

        bool executeTaskCopy(CopyTaskResources & res) override { return copy(res); }
        bool executeTaskRemove(RemoveTaskResources & res) override { return remove(res); }

        bool executeTaskDirCompare(DirectoryCompareTaskResources & res) override
        {
            return compareDirectoryContents(res);
        }

        bool executeTaskFileCompare(FileCompareTaskResources & res) override
        {
            return compareFileContents(res);
        }

        void printStatusUpdateIfTime() override;

      private:
        CopyTasker m_copyTasker;
        RemoveTasker m_removeTasker;
        FileCompareTasker m_fileCompareTasker;
        DirectoryCompareTasker m_dirCompareTasker;

        std::size_t m_statusPeriodMs;
        const Clock_t::time_point m_startTime;
    };

} // namespace backup

#endif // BACKUP_TOOL_HPP_INCLUDED
