// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
//
// backup-tool.cpp
//
#include "backup-tool.hpp"

#include "str-util.hpp"
#include "util.hpp"

#include <algorithm>

namespace backup
{

    BackupTool::BackupTool(const std::vector<std::string> & args)
        : BaseFileOperations(args)
        , m_copyTasker(*this, options().thread_counts.copy)
        , m_removeTasker(*this, options().thread_counts.remove)
        , m_fileCompareTasker(*this, options().thread_counts.file_compare)
        , m_dirCompareTasker(*this, options().thread_counts.dir_compare)
        , m_statusPeriodMs(5000)
        , m_startTime(Clock_t::now()) // intentionally start time after all the resource init
    {}

    void BackupTool::run()
    {
        bool wasExceptionError{ true };

        try
        {
            startAndWaitForAllThreadsToFinish();
            handleAnyExceptions();
            wasExceptionError = false;
        }
        catch (const keypress_caused_abort & ex)
        {
            printLine(strutil::toWideString(ex.what()), Color::Red);
        }
        catch (const std::exception & ex)
        {
            printLine(
                (L"Fatal Exception: \"" + strutil::toWideString(ex.what()) + L"\""), Color::Red);
        }
        catch (...)
        {
            printLine(L"Fatal Exception: (unknown)", Color::Red);
        }

        printFinalResults(wasExceptionError);
    }

    void BackupTool::printFinalResults(const bool didAbortEarly)
    {
        // capture the run time before spending lots of time formatting the counter strings
        const std::wstring timeElapsedStr{ prettyTimeDurationString(m_startTime) };

        const CounterResults counterResults{ printCounterResults() };

        Color resultColor{ Color::Default };
        std::wstring resultStr;

        if (didAbortEarly)
        {
            resultStr   = L"ERROR (something casued the app to abort)";
            resultColor = Color::Red;
        }
        else if (options().job == Job::Compare)
        {
            if (counterResults.errors || counterResults.mismatches)
            {
                resultStr   = L"NOT equal";
                resultColor = Color::Red;
            }
            else
            {
                resultStr   = L"Equal";
                resultColor = Color::Green;
            }
        }
        else if (options().job == Job::Copy)
        {
            if (counterResults.errors)
            {
                resultStr   = L"FAIL";
                resultColor = Color::Red;
            }
            else if (counterResults.copies || options().dry_run)
            {
                resultStr   = L"Success";
                resultColor = Color::Green;
            }
            else
            {
                resultStr   = L"Nothing to copy!";
                resultColor = Color::Yellow;
            }
        }
        else // Cull
        {
            if (counterResults.errors)
            {
                resultStr   = L"FAIL";
                resultColor = Color::Red;
            }
            else if (counterResults.removes || options().dry_run)
            {
                resultStr   = L"Success";
                resultColor = Color::Green;
            }
            else
            {
                resultStr   = L"No extras to delete!";
                resultColor = Color::Yellow;
            }
        }

        if (options().dry_run)
        {
            resultStr += L" (dryrun)";
        }

        if ((options().job != Job::Cull) && (options().skip_file_read))
        {
            resultStr += L" (skip_file_read -which means only file sizes were checked)";
        }

        if (options().quiet)
        {
            disableQuietOptionToPrintFinalResults();
            printLine(resultStr, resultColor);
        }
        else
        {
            printLine(resultStr, resultColor);
            printLine(timeElapsedStr);
        }
    }

    void BackupTool::startAndWaitForAllThreadsToFinish()
    {
        scheduleDirectoryCompare(
            EntryConstRefDPair_t{ options().entry_dpair.src, options().entry_dpair.dst });

        m_fileCompareTasker.start();

        m_dirCompareTasker.start();
        m_dirCompareTasker.waitUntilFinished();

        // copy and remove threads must wait for all dirComparer threads to finish before starting,
        // because they will change directory contents while other threads are iterating over them
        m_copyTasker.start();
        m_removeTasker.start();

        m_fileCompareTasker.waitUntilFinished();

        m_copyTasker.waitUntilFinished();
        m_removeTasker.waitUntilFinished();
    }

    void BackupTool::notifyAll()
    {
        m_fileCompareTasker.notifyAll();
        m_dirCompareTasker.notifyAll();
        m_copyTasker.notifyAll();
        m_removeTasker.notifyAll();
    }

    void BackupTool::scheduleFileCompare(const EntryConstRefDPair_t & entryDPair)
    {
        m_fileCompareTasker.enqueue(entryDPair);
    }

    void BackupTool::scheduleDirectoryCompare(const EntryConstRefDPair_t & entryDPair)
    {
        m_dirCompareTasker.enqueue(entryDPair);
    }

    void BackupTool::scheduleFileCopy(const EntryConstRefDPair_t & entryDPair)
    {
        m_copyTasker.enqueue(entryDPair);
    }

    void BackupTool::scheduleFileRemove(const EntryConstRefDPair_t & entryDPair)
    {
        m_removeTasker.enqueue(entryDPair);
    }

    void BackupTool::printStatusUpdateIfTime()
    {
        if ((elapsedCountMs(m_startTime) < m_statusPeriodMs) ||
            (elapsedCountMs(lastPrintTime()) < m_statusPeriodMs))
        {
            return;
        }

        // wait three seconds longer between consecutive status prints
        m_statusPeriodMs = std::clamp((m_statusPeriodMs + 3000_st), 5000_st, 20'000_st);

        static std::size_t dirPrevCompletedCount{ 0 };
        static std::size_t filePrevCompletedCount{ 0 };
        static std::size_t copyPrevCompletedCount{ 0 };
        static std::size_t removePrevCompletedCount{ 0 };

        const TaskQueueStatus dirStatus{ m_dirCompareTasker.status() };
        const TaskQueueStatus fileStatus{ m_fileCompareTasker.status() };
        const TaskQueueStatus copyStatus{ m_copyTasker.status() };
        const TaskQueueStatus removeStatus{ m_removeTasker.status() };

        if (m_dirCompareTasker.isFinished() && m_fileCompareTasker.isFinished() &&
            m_copyTasker.isFinished() && m_removeTasker.isFinished())
        {
            return;
        }

        const std::wstring dirProgressStr{ std::to_wstring(dirStatus.completed_count) };

        const std::wstring fileProgressStr{ calcPercentString(
            fileStatus.progress_sum, fileStatus.resource_busy_count) };

        const std::wstring copyProgressStr{ fileSizeToString(
            static_cast<std::size_t>(copyStatus.progress_sum)) };

        std::wstring removeProgressStr{ L"?" };
        {
            using namespace std::chrono;

            const Progress_t avgStartTimeNsSinceEpoch{ calcPercent(
                removeStatus.progress_sum, removeStatus.resource_busy_count) };

            if (avgStartTimeNsSinceEpoch > m_startTime.time_since_epoch().count())
            {
                const Clock_t::time_point avgStartTime(
                    milliseconds(avgStartTimeNsSinceEpoch * 1000));
                removeProgressStr = prettyTimeDurationString(avgStartTime);
            }
        }

        std::wostringstream ss;
        ss << std::setw(6) << std::right << prettyTimeDurationString(m_startTime);

        // if (wasKeyPressed)
        //{
        //    ss << L" and you're already mashing the keyboard like a sexually frustrated monkey.";
        //}
        // else
        {
            ss << L" and still working.";
        }

        ss << L"..  Here, looking at numbers will make you feel better: ";

        bool wereAnyPrinted{ false };
        auto streamTaskQueueStatus = [&](const std::wstring & name,
                                         const TaskQueueStatus & status,
                                         const std::size_t prevCompletedCount,
                                         const std::wstring & progress) {
            if ((0 == status.queue_size) && (0 == status.resource_busy_count))
            {
                return;
            }

            std::wstring summaryStr;

            if (wereAnyPrinted)
            {
                summaryStr += L", ";
            }

            summaryStr += name;
            summaryStr += L"=";

            if ((status.completed_count == prevCompletedCount) && (0 == status.completed_count) &&
                (0 == status.resource_busy_count) && (0 == status.progress_sum))
            {
                if (status.queue_size > 0)
                {
                    return;
                }

                summaryStr += std::to_wstring(status.queue_size);
                summaryStr += L"_queued_but_not_started";
            }
            else if (status.completed_count == prevCompletedCount)
            {
                summaryStr += std::to_wstring(status.completed_count);
                summaryStr += L" (queued=";
                summaryStr += std::to_wstring(status.queue_size);
                summaryStr += L", busy=";
                summaryStr += std::to_wstring(status.resource_busy_count);
                summaryStr += L", ";
                summaryStr += progress;
                summaryStr += L")";
            }
            else
            {
                summaryStr += std::to_wstring(status.completed_count);
            }

            ss << summaryStr;

            wereAnyPrinted = true;
        };

        streamTaskQueueStatus(L"Dirs", dirStatus, dirPrevCompletedCount, dirProgressStr);
        streamTaskQueueStatus(L"Files", fileStatus, filePrevCompletedCount, fileProgressStr);
        streamTaskQueueStatus(L"Copies", copyStatus, copyPrevCompletedCount, copyProgressStr);
        streamTaskQueueStatus(
            L"Deletes", removeStatus, removePrevCompletedCount, removeProgressStr);

        printLine(ss.str(), Color::Gray);

        dirPrevCompletedCount    = dirStatus.completed_count;
        filePrevCompletedCount   = fileStatus.completed_count;
        copyPrevCompletedCount   = copyStatus.completed_count;
        removePrevCompletedCount = removeStatus.completed_count;
    }

} // namespace backup
