// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
//
// base-file-operations.cpp
//
#include "base-file-operations.hpp"

#include "str-util.hpp"
#include "util.hpp"

#include <algorithm>
#include <cassert>
#include <future>

namespace backup
{

    BaseFileOperations::BaseFileOperations(const std::vector<std::string> & args)
        : BaseCountersAndErrors(args)
        , m_subThreadExceptions()
    {}

    bool BaseFileOperations::copy(CopyTaskResources & resources)
    {
        try
        {
            const EntryConstRefDPair_t entryDPair{ resources.entry_dpair.src,
                                                   resources.entry_dpair.dst };

            assert(entryDPair.src.which_dir == WhichDir::Source);
            assert(entryDPair.dst.which_dir == WhichDir::Destination);
            assert(!entryDPair.src.path.empty() && !entryDPair.dst.path.empty());
            assert(entryDPair.src.is_file == entryDPair.dst.is_file);

            const bool alreadyExists{ existsIgnoringErrors(entryDPair.dst.path, false) };
            if (alreadyExists)
            {
                if (!remove(resources))
                {
                    return false;
                }
            }

            bool success{ true };

            if (entryDPair.src.is_file)
            {
                success = copyAndCountFile(entryDPair, resources.progress);
            }
            else
            {
                success = copyDirectoryDeep(entryDPair, resources.progress);
            }

            if (success)
            {
                std::wstring detailStr{ L"(" };
                if (options().dry_run)
                {
                    detailStr += L"DryRun";
                }
                else
                {
                    detailStr += fileSizeToString(static_cast<std::size_t>(resources.progress));
                }
                detailStr += L")";

                printEntryEvent(L"Copied", detailStr, entryDPair.src);
            }

            return success;
        }
        catch (...)
        {
            m_subThreadExceptions.add(std::current_exception());
            return false;
        }
    }

    bool BaseFileOperations::remove(TaskResourcesBase & resources)
    {
        try
        {
            const auto & entry{ resources.entry_dpair.dst };

            assert(!entry.path.empty());
            assert(entry.which_dir == WhichDir::Destination);

            std::wstring detailStr{ L"(" };

            if (options().dry_run)
            {
                detailStr += L"DryRun";
            }
            else
            {
                assert(existsIgnoringErrors(entry.path, true));

                ErrorCode_t errorCodeRemove;
                const auto removedCount{ fs::remove_all(entry.path, errorCodeRemove) };
                if (!printAndCountErrorCodeIf(errorCodeRemove, Error::Remove, entry))
                {
                    return false;
                }

                if (removedCount == 0)
                {
                    printAndCountError(Error::Remove, entry, L"remove_all() returned zero");
                    return false;
                }

                assert(!existsIgnoringErrors(entry.path, false));

                detailStr += L"x";
                detailStr += std::to_wstring(removedCount);
            }

            detailStr += L")";
            printEntryEvent(L"Deleted", detailStr, entry);
            countRemove(entry);
            return true;
        }
        catch (...)
        {
            m_subThreadExceptions.add(std::current_exception());
            return false;
        }
    }

    bool BaseFileOperations::compareFileContents(FileCompareTaskResources & resources)
    {
        try
        {
            if (options().skip_file_read)
            {
                return true;
            }

            const EntryConstRefDPair_t entryDPair{ resources.entry_dpair.src,
                                                   resources.entry_dpair.dst };

            auto & fileDPair{ resources.file_dpair };

            assert(!entryDPair.src.path.empty() && !entryDPair.dst.path.empty());
            assert(entryDPair.src.is_file);
            assert(entryDPair.dst.is_file);
            assert(entryDPair.src.size > 0);
            assert(entryDPair.dst.size > 0);
            assert(entryDPair.src.size == entryDPair.dst.size);
            assert(entryDPair.src.which_dir == WhichDir::Source);
            assert(entryDPair.dst.which_dir == WhichDir::Destination);

            if (!printAndCountStreamErrorIf(fileDPair.src.stream, Error::Open, entryDPair.src))
            {
                return false;
            }

            if (!printAndCountStreamErrorIf(fileDPair.dst.stream, Error::Open, entryDPair.dst))
            {
                return false;
            }

            std::size_t remainingSize{ entryDPair.src.size };
            std::size_t readSize{ std::min(remainingSize, FileReadResources::min_read_size) };

            while (remainingSize > 0)
            {
                assert(readSize <= remainingSize);
                assert(readSize <= FileReadResources::max_read_size);

                // start to read src file with new thread
                auto srcReadFuture{ std::async(
                    std::launch::async,
                    &BaseFileOperations::fileRead,
                    this,
                    entryDPair.src,
                    readSize,
                    std::ref(fileDPair.src)) };

                // start and finish reading dst file from this thread now
                const bool dstReadResult{ fileRead(entryDPair.dst, readSize, fileDPair.dst) };

                // blocking wait for the other thread, after this line both threads are finished
                const bool srcReadResult{ srcReadFuture.get() };

                if (!srcReadResult || !dstReadResult)
                {
                    return false;
                }

                resources.progress = static_cast<Progress_t>(
                    (static_cast<double>(entryDPair.src.size - remainingSize) /
                     static_cast<double>(entryDPair.src.size)) *
                    100.0);

                if ((std::memcmp(&fileDPair.src.buffer[0], &fileDPair.dst.buffer[0], readSize)) !=
                    0)
                {
                    // handling this mistmatch might enqueue a copy or delete task
                    // another thread might start doing that before resource.teardown() closes the
                    // file so our file might still be open when another thread tries to copy or
                    // delete it so we must teardown now before calling handleMismatch() and we must
                    // be sure to simply return afterwards and not use resources after
                    resources.teardown();
                    handleMismatch(Mismatch::Modified, entryDPair);
                    return false;
                }

                remainingSize -= readSize;

                readSize *= 2;

                if (readSize > FileReadResources::max_read_size)
                {
                    readSize = FileReadResources::max_read_size;
                }

                if (readSize > remainingSize)
                {
                    readSize = remainingSize;
                }
            }

            return true;
        }
        catch (...)
        {
            m_subThreadExceptions.add(std::current_exception());
            return false;
        }
    }

    bool BaseFileOperations::compareDirectoryContents(DirectoryCompareTaskResources & resources)
    {
        try
        {
            // don't exit early here even if dry run so that we can verify the directory traversal

            assert(resources.entry_dpair.src.which_dir == WhichDir::Source);
            assert(!resources.entry_dpair.src.is_file);
            assert(!resources.entry_dpair.src.path.empty());
            assert(resources.entry_dpair.src.size == 0);
            //
            assert(resources.entry_dpair.dst.which_dir == WhichDir::Destination);
            assert(!resources.entry_dpair.dst.is_file);
            assert(!resources.entry_dpair.dst.path.empty());
            assert(resources.entry_dpair.dst.size == 0);

            // start to parse src directory with new thread
            auto srcParseFuture{ std::async(
                std::launch::async,
                &BaseFileOperations::makeEntrysForAllInDirectory,
                this,
                resources.entry_dpair.src,
                std::ref(resources.file_entrys_dpair.src),
                std::ref(resources.dir_entrys_dpair.src)) };

            // start and finish parsing dst directory with this thread now
            const bool dstParseSuccess{ makeEntrysForAllInDirectory(
                resources.entry_dpair.dst,
                resources.file_entrys_dpair.dst,
                resources.dir_entrys_dpair.dst) };

            // wait for the src parse thread to finish
            const bool srcParseSuccess{ srcParseFuture.get() };

            if (!srcParseSuccess || !dstParseSuccess)
            {
                return false;
            }

            const bool areAnyFilesToCompare{ !resources.file_entrys_dpair.src.empty() ||
                                             !resources.file_entrys_dpair.dst.empty() };

            const bool areAnyDirsToCompare{ !resources.dir_entrys_dpair.src.empty() ||
                                            !resources.dir_entrys_dpair.dst.empty() };

            if (!areAnyFilesToCompare && !areAnyDirsToCompare)
            {
                return true;
            }

            const std::size_t srcFileCount{ resources.file_entrys_dpair.src.size() };
            const std::size_t srcDirCount{ resources.dir_entrys_dpair.src.size() };
            if (options().verbose && (srcFileCount + srcDirCount) >= 5000)
            {
                std::wostringstream ss;
                ss.imbue(std::locale("")); // this is only to put commas in the big numbers

                ss << L"dir has an unusually high number of entries: " << L"dirs=" << srcDirCount
                   << L", files=" << srcFileCount;

                printWarningEvent(
                    L"BigDir",
                    WhichDir::Source,
                    resources.entry_dpair.src.is_file,
                    resources.entry_dpair.src.path.wstring(),
                    ss.str());
            }

            // start to compare file entrys with new thread (if needed)
            std::future<bool> fileComapreFuture;
            if (areAnyFilesToCompare)
            {
                fileComapreFuture = std::async(
                    std::launch::async,
                    &BaseFileOperations::comparEntrysWithSameType,
                    this,
                    resources.entry_dpair,
                    resources.file_entrys_dpair.src,
                    resources.file_entrys_dpair.dst);
            }

            // start and finish comparing dir entrys with this thread now (if needed)
            bool dirCompareSuccess{ true };
            if (areAnyDirsToCompare)
            {
                dirCompareSuccess = comparEntrysWithSameType(
                    resources.entry_dpair,
                    resources.dir_entrys_dpair.src,
                    resources.dir_entrys_dpair.dst);
            }

            // wait for the file parse thread to finish (if needed)
            const bool fileCompareSuccess{ fileComapreFuture.valid() ? fileComapreFuture.get()
                                                                     : true };

            return (fileCompareSuccess && dirCompareSuccess);
        }
        catch (...)
        {
            m_subThreadExceptions.add(std::current_exception());
            return false;
        }
    }

    void BaseFileOperations::handleAnyExceptions()
    {
        printLine(m_subThreadExceptions.makeSummaryString(), Color::Red);
        m_subThreadExceptions.reThrowFirst();
    }

    bool BaseFileOperations::fileRead(
        const Entry & entry, const std::size_t readSize, FileReadResources & resources)
    {
        try
        {
            assert(entry.is_file);
            assert(!entry.path.empty());
            assert(entry.size > 0);

            assert(readSize > 0);
            assert(readSize <= entry.size);
            assert(readSize <= resources.buffer.size());

            assert(!resources.buffer.empty());
            assert(resources.min_read_size <= resources.max_read_size);
            assert(resources.stream);
            assert(resources.stream.is_open());

            resources.stream.read(&resources.buffer[0], static_cast<std::streamsize>(readSize));
            return printAndCountStreamErrorIf(resources.stream, Error::Read, entry);
        }
        catch (...)
        {
            m_subThreadExceptions.add(std::current_exception());
            return false;
        }
    }

    bool BaseFileOperations::makeEntrysForAllInDirectory(
        const Entry & dirEntry, EntryVec_t & fileEntrys, EntryVec_t & dirEntrys)
    {
        try
        {
            assert(!dirEntry.path.empty());
            assert(!dirEntry.is_file);
            assert(dirEntry.size == 0);

            ErrorCode_t errorCodeMakeDirIter;
            fs::directory_iterator iter(dirEntry.path, errorCodeMakeDirIter);
            if (!printAndCountErrorCodeIf(errorCodeMakeDirIter, Error::DirIterMake, dirEntry))
            {
                return false;
            }

            const fs::directory_iterator iterEnd;

            while (iter != iterEnd)
            {
                makeAndStoreEntry(dirEntry.which_dir, *iter, fileEntrys, dirEntrys);
                incrementDirectoryIterator(dirEntry, iter);
            }

            // sorting by name is required by comparEntrysWithSameType()
            // processing things in alphabetical order also helps the app behave in the expected way
            auto sortEntrysByName = [](EntryVec_t & vec) {
                if (vec.size() < 2)
                {
                    return;
                }

                std::sort(std::begin(vec), std::end(vec), [](const auto & A, const auto & B) {
                    return (A.name < B.name);
                });
            };

            sortEntrysByName(fileEntrys);
            sortEntrysByName(dirEntrys);

            return true;
        }
        catch (...)
        {
            m_subThreadExceptions.add(std::current_exception());
        }

        return false;
    }

    bool BaseFileOperations::comparEntrysWithSameType(
        const EntryDPair_t & parentEntryDPair,
        const EntryVec_t & srcEntrys,
        const EntryVec_t & dstEntrys)
    {
        try
        {
            assert(parentEntryDPair.src.which_dir == WhichDir::Source);
            assert(!parentEntryDPair.src.is_file);
            assert(!parentEntryDPair.src.path.empty());
            assert(parentEntryDPair.src.size == 0);
            //
            assert(parentEntryDPair.dst.which_dir == WhichDir::Destination);
            assert(!parentEntryDPair.dst.is_file);
            assert(!parentEntryDPair.dst.path.empty());
            assert(parentEntryDPair.dst.size == 0);

            auto srcIter{ std::begin(srcEntrys) };
            const auto srcIterEnd{ std::end(srcEntrys) };

            auto dstIter{ std::begin(dstEntrys) };
            const auto dstIterEnd{ std::end(dstEntrys) };

            const EntryDPair_t emptyEntryDPair{ Entry(WhichDir::Source, false, fs::path(), 0),
                                                Entry(
                                                    WhichDir::Destination, false, fs::path(), 0) };

            while ((srcIterEnd != srcIter) || (dstIterEnd != dstIter))
            {
                // only one of these two can possibly be the empty entry
                const EntryConstRefDPair_t entryDPair{
                    ((srcIterEnd == srcIter) ? emptyEntryDPair.src : *srcIter),
                    ((dstIterEnd == dstIter) ? emptyEntryDPair.dst : *dstIter)
                };

                const int nameCompareResult = [&]() {
                    if (srcIterEnd == srcIter)
                    {
                        return 1;
                    }
                    else if (dstIterEnd == dstIter)
                    {
                        return -1;
                    }
                    else
                    {
                        return entryDPair.src.name.compare(entryDPair.dst.name);
                    }
                }();

                if (nameCompareResult > 0)
                {
                    // extra case

                    // In this case the src entry is not correct, and actually never could be.  So
                    // use the src parent dir entry as a subsitute because that will help produce
                    // better logging information.
                    handleMismatch(
                        Mismatch::Extra,
                        EntryConstRefDPair_t{ parentEntryDPair.src, entryDPair.dst });

                    ++dstIter;
                }
                else if (nameCompareResult < 0)
                {
                    // missing case

                    if (options().job != Job::Cull)
                    {
                        // In this case the dst entry is not correct and a replacement must be
                        // created. The dst Entry is created to be exactly the same as the src,
                        // except for the path. This new dst Entry could be either a file or dir,
                        // and could either exist or not.
                        const Entry fixedDstEntry(
                            WhichDir::Destination,
                            entryDPair.src.is_file,
                            (parentEntryDPair.dst.path / entryDPair.src.name),
                            entryDPair.src.size);

                        handleMismatch(
                            Mismatch::Missing,
                            EntryConstRefDPair_t{ entryDPair.src, fixedDstEntry });
                    }

                    ++srcIter;
                }
                else
                {
                    // two entrys with same name and same type case

                    if (!entryDPair.src.is_file || (options().job != Job::Cull))
                    {
                        compareEntrysWithSameTypeAndName(entryDPair);
                    }

                    ++srcIter;
                    ++dstIter;
                }
            }

            return true;
        }
        catch (...)
        {
            m_subThreadExceptions.add(std::current_exception());
        }

        return false;
    }

    void BaseFileOperations::compareEntrysWithSameTypeAndName(
        const EntryConstRefDPair_t & entryDPair)
    {
        assert(entryDPair.src.which_dir == WhichDir::Source);
        assert(entryDPair.dst.which_dir == WhichDir::Destination);
        assert(!entryDPair.src.path.empty() && !entryDPair.dst.path.empty());
        assert(entryDPair.src.is_file == entryDPair.dst.is_file);
        assert(entryDPair.src.name == entryDPair.dst.name);
        assert(entryDPair.src.extension == entryDPair.dst.extension);

        if (entryDPair.src.is_file)
        {
            if (entryDPair.src.size == entryDPair.dst.size)
            {
                if (!options().skip_file_read && (entryDPair.src.size > 0))
                {
                    scheduleFileCompare(entryDPair);
                }
            }
            else
            {
                handleMismatch(Mismatch::Size, entryDPair);
            }
        }
        else
        {
            scheduleDirectoryCompare(entryDPair);
        }
    }

    void BaseFileOperations::makeAndStoreEntry(
        const WhichDir whichDir,
        const fs::directory_entry & dirEntry,
        EntryVec_t & fileEntrys,
        EntryVec_t & dirEntrys)
    {
        bool isFile{ false };
        bool hasSize{ false };
        if (!setTypeOrHandleError(whichDir, dirEntry, isFile, hasSize))
        {
            return;
        }

        std::size_t size{ 0 };
        if (isFile && hasSize)
        {
            ErrorCode_t errorCode;
            size = getSizeCommon(dirEntry, errorCode);
            if (errorCode)
            {
                const Entry tempEntry(whichDir, isFile, dirEntry.path(), 0);
                printAndCountErrorCodeIf(errorCode, Error::Size, tempEntry);
                return;
            }
        }

        EntryVec_t & vec{ (isFile) ? fileEntrys : dirEntrys };
        Entry & entry{ vec.emplace_back(whichDir, isFile, dirEntry.path(), size) };

        count(entry);

        if (options().verbose && (entry.size > 10'000'000'000))
        {
            printWarningEvent(
                L"BigFile",
                entry.which_dir,
                entry.is_file,
                entry.path.wstring(),
                fileSizeToString(entry.size));
        }
    }

    void BaseFileOperations::incrementDirectoryIterator(
        const Entry & parentDirEntry, fs::directory_iterator & iter)
    {
        ErrorCode_t errorCode;
        iter.increment(errorCode);
        if (errorCode)
        {
            const std::wstring errorMessage{ L"Path in that dir that caused the error=\"" +
                                             iter->path().wstring() + L"\"" };

            printAndCountErrorCodeIf(errorCode, Error::DirIterInc, parentDirEntry, errorMessage);
            iter = fs::directory_iterator();
        }
    }

    void BaseFileOperations::handleMismatch(
        const Mismatch mismatch,
        const EntryConstRefDPair_t & entryDPair,
        const std::wstring & message)
    {
        if ((Mismatch::Extra == mismatch) && options().ignore_extra)
        {
            return;
        }

        const auto job{ options().job };

        if (job == Job::Copy)
        {
            if (mismatch != Mismatch::Extra)
            {
                printAndCountMismatch(mismatch, entryDPair.src, message);
                scheduleFileCopy(entryDPair);
            }
        }
        else if (job == Job::Cull)
        {
            if (mismatch == Mismatch::Extra)
            {
                printAndCountMismatch(mismatch, entryDPair.dst, message);
                scheduleFileRemove(entryDPair);
            }
        }
        else // Job::Compare case here
        {
            const WhichDir whichDirToLog{ (mismatch == Mismatch::Missing) ? WhichDir::Source
                                                                          : WhichDir::Destination };

            printAndCountMismatch(mismatch, entryDPair.get(whichDirToLog), message);
        }
    }

    bool BaseFileOperations::setTypeOrHandleError(
        const WhichDir whichDir,
        const fs::directory_entry & dirEntry,
        bool & isFile,
        bool & hasSize)
    {
        ErrorCode_t errorCodeSymlinkStatus;
        const fs::file_status symlinkStatus{ dirEntry.symlink_status(errorCodeSymlinkStatus) };
        if (errorCodeSymlinkStatus)
        {
            const Entry tempEntry(whichDir, false, dirEntry.path(), 0);
            printAndCountErrorCodeIf(errorCodeSymlinkStatus, Error::SymlinkStatus, tempEntry);
            return false;
        }

        ErrorCode_t errorCodeNormalStatus;
        const fs::file_status normalStatus{ dirEntry.status(errorCodeNormalStatus) };
        if (errorCodeNormalStatus)
        {
            const Entry tempEntry(whichDir, false, dirEntry.path(), 0);
            printAndCountErrorCodeIf(errorCodeNormalStatus, Error::Status, tempEntry);
            return false;
        }

        const bool isRegularFile{ fs::is_regular_file(symlinkStatus) };
        const bool isDirectory{ fs::is_directory(symlinkStatus) };
        const bool isSymlink{ fs::is_symlink(symlinkStatus) };

        // symlinks do exist on Windows (that are neither shortcuts or junctions) but are not
        // supported this macro mess tries to detect if running on windows, and if so, considers
        // symlinks errors see the very extensive comments at the top of filesystem-common.hpp
        if (is_running_on_windows)
        {
            isFile  = isRegularFile;
            hasSize = isRegularFile;
        }
        else
        {
            isFile  = (isRegularFile || isSymlink);
            hasSize = isRegularFile;
        }

        std::wstring symlinkTypeStr;
        if (isSymlink)
        {
            symlinkTypeStr = L"symlink to a ";
            symlinkTypeStr += toString(normalStatus.type());
            symlinkTypeStr += L" at \"";

            ErrorCode_t errorCodeTemp;

            const std::wstring linkedPathStr{
                fs::read_symlink(dirEntry.path(), errorCodeTemp).wstring()
            };

            if (errorCodeTemp)
            {
                symlinkTypeStr += L"error_unable_to_follow_symlink";
            }
            else
            {
                symlinkTypeStr += linkedPathStr;
            }

            symlinkTypeStr += L"\"";
        }

        const bool isFileOrDirUnknown{ isFile == isDirectory };
        const bool isSymlinkTypeUnknown{ isSymlink && !isFile };
        const bool willShowUnknown{ !options().ignore_unknown };

        if ((isFileOrDirUnknown || isSymlinkTypeUnknown) && willShowUnknown)
        {
            std::wstring errorMessage;
            errorMessage += L"unsupported_type: ";
            errorMessage += toString(symlinkStatus.type());

            if (!symlinkTypeStr.empty())
            {
                errorMessage += L": ";
                errorMessage += symlinkTypeStr;
            }

            const Entry tempEntry(whichDir, false, dirEntry.path(), 0);
            printAndCountError(Error::UnsupportedType, tempEntry, errorMessage);
            return false;
        }
        else
        {
            if (options().verbose && isSymlink)
            {
                printWarningEvent(
                    L"Symlink", whichDir, isFile, dirEntry.path().wstring(), symlinkTypeStr);
            }

            return true;
        }
    }

    bool BaseFileOperations::copyAndCountFile(
        const EntryConstRefDPair_t & entryDPair, Progress_t & byteCounter)
    {
        if (!options().dry_run)
        {
            ErrorCode_t errorCode;
            copyFileCommon(entryDPair.src.path, entryDPair.dst.path, errorCode);
            if (!printAndCountErrorCodeIf(errorCode, Error::Copy, entryDPair.src))
            {
                return false;
            }
        }

        countCopy(entryDPair.src);
        byteCounter += entryDPair.src.size;
        return true;
    }

    bool BaseFileOperations::copyAndCountDirectoryShallow(const EntryConstRefDPair_t & entryDPair)
    {
        if (!options().dry_run)
        {
            ErrorCode_t errorCode;
            const bool createDirectorySuccess{ fs::create_directory(
                entryDPair.dst.path, errorCode) };

            if (!printAndCountErrorCodeIf(
                    errorCode,
                    Error::CreateDirectory,
                    entryDPair.dst,
                    entryDPair.src.path.wstring()))
            {
                return false;
            }

            if (!createDirectorySuccess)
            {
                printAndCountError(
                    Error::CreateDirectory, entryDPair.dst, entryDPair.src.path.wstring());
                return false;
            }

            assert(existsIgnoringErrors(entryDPair.dst.path, false));
        }

        countCopy(entryDPair.src);
        return true;
    }

    bool BaseFileOperations::copyDirectoryDeep(
        const EntryConstRefDPair_t & parentDirEntryDPair, Progress_t & byteCounter)
    {
        if (!copyAndCountDirectoryShallow(parentDirEntryDPair))
        {
            return false;
        }

        bool wereAnyErrors{ false };

        EntryVec_t fileEntrys;
        EntryVec_t dirEntrys;
        if (!makeEntrysForAllInDirectory(parentDirEntryDPair.src, fileEntrys, dirEntrys))
        {
            wereAnyErrors = true;
        }

        auto doCopyWork = [&](const Entry & childSrcEntry) {
            // dst is the same as the src except of course for the path, see
            // comparEntrysWithSameType() dst could be either a file or dir, and could either exist
            // or not
            const Entry childDstEntry(
                WhichDir::Destination,
                childSrcEntry.is_file,
                (parentDirEntryDPair.dst.path / childSrcEntry.name),
                childSrcEntry.size);

            const EntryConstRefDPair_t newEntryDPair{ childSrcEntry, childDstEntry };

            if (childSrcEntry.is_file)
            {
                if (!copyAndCountFile(newEntryDPair, byteCounter))
                {
                    wereAnyErrors = true;
                }
            }
            else
            {
                if (!copyDirectoryDeep(newEntryDPair, byteCounter))
                {
                    wereAnyErrors = true;
                }
            }
        };

        for (const auto & childFileSrcEntry : fileEntrys)
        {
            doCopyWork(childFileSrcEntry);
        }

        fileEntrys.clear();
        fileEntrys.shrink_to_fit();

        for (const auto & childDirSrcEntry : dirEntrys)
        {
            doCopyWork(childDirSrcEntry);
        }

        return !wereAnyErrors;
    }

} // namespace backup
