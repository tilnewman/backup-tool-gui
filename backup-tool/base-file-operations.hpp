#ifndef BACKUP_BASE_FILE_OPERATIONS_HPP_INCLUDED
#define BACKUP_BASE_FILE_OPERATIONS_HPP_INCLUDED
//
// base-file-operations.hpp
//
#include "base-counters-and-errors.hpp"
#include "task-resources.hpp"

namespace backup
{

    class BaseFileOperations : public BaseCountersAndErrors
    {
      protected:
        BaseFileOperations(const std::vector<std::string> & args);
        ~BaseFileOperations() = default;

        bool copy(CopyTaskResources & resources);
        bool remove(TaskResourcesBase & resources);
        bool compareFileContents(FileCompareTaskResources & resources);
        bool compareDirectoryContents(DirectoryCompareTaskResources & resources);

        virtual void scheduleFileCompare(const EntryConstRefDPair_t & entryDPair)      = 0;
        virtual void scheduleDirectoryCompare(const EntryConstRefDPair_t & entryDPair) = 0;
        virtual void scheduleFileCopy(const EntryConstRefDPair_t & entryDPair)         = 0;
        virtual void scheduleFileRemove(const EntryConstRefDPair_t & entryDPair)       = 0;

        inline bool haveAnyExceptionsBeenThrown()
        {
            return (m_subThreadExceptions.wereAnyThrown());
        }

        void handleAnyExceptions();

      private:
        bool fileRead(
            const Entry & entry, const std::size_t readSize, FileReadResources & resources);

        bool makeEntrysForAllInDirectory(
            const Entry & dirEntry, EntryVec_t & fileEntrys, EntryVec_t & dirEntrys);

        bool comparEntrysWithSameType(
            const EntryDPair_t & parentEntryDPair,
            const EntryVec_t & srcEntrys,
            const EntryVec_t & dstEntrys);

        void compareEntrysWithSameTypeAndName(const EntryConstRefDPair_t & entryDPair);

        void makeAndStoreEntry(
            const WhichDir whichDir,
            const fs::directory_entry & dirEntry,
            EntryVec_t & fileEntrys,
            EntryVec_t & dirEntrys);

        void
            incrementDirectoryIterator(const Entry & parentDirEntry, fs::directory_iterator & iter);

        void handleMismatch(
            const Mismatch mismatch,
            const EntryConstRefDPair_t & entryDPair,
            const std::wstring & message = L"");

        bool setTypeOrHandleError(
            const WhichDir whichDir,
            const fs::directory_entry & dirEntry,
            bool & isFile,
            bool & hasSize);

        bool copyAndCountFile(const EntryConstRefDPair_t & entryDPair, Progress_t & byteCounter);

        bool copyAndCountDirectoryShallow(const EntryConstRefDPair_t & entryDPair);

        bool copyDirectoryDeep(const EntryConstRefDPair_t & entryDPair, Progress_t & byteCounter);

      private:
        ThreadExceptions m_subThreadExceptions;
    };

} // namespace backup

#endif // BACKUP_BASE_FILE_OPERATIONS_HPP_INCLUDED
