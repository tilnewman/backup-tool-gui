// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
//
// base-counters-and-errors.cpp
//
#include "base-counters-and-errors.hpp"

#include "str-util.hpp"
#include "util.hpp"

#include <algorithm>
#include <cassert>

namespace backup
{

    BaseCountersAndErrors::BaseCountersAndErrors(const std::vector<std::string> & args)
        : BaseOptionsAndOutput(args)
        , m_copyCounter(L"Copied")
        , m_removeCounter(L"Deleted")
        , m_mismatchCounter(L"Mismatches", Color::Yellow, L"Mismatch Categories", Color::Yellow)
        , m_srcTreeCounter(L"Source Tree", Color::Default, L"Errors", Color::Red)
        , m_dstTreeCounter(L"Destination Tree", Color::Default, L"Errors", Color::Red)
    {}

    void BaseCountersAndErrors::count(const Entry & entry)
    {
        if (WhichDir::Source == entry.which_dir)
        {
            m_srcTreeCounter.incrementByEntry(entry);
        }
        else
        {
            m_dstTreeCounter.incrementByEntry(entry);
        }
    }

    void BaseCountersAndErrors::printAndCountError(
        const Error error, const Entry & entry, const std::wstring & message)
    {
        const bool isFailToAccess{ (
            isAccessError(error) || (message.find(L"denied") < message.size()) ||
            (message.find(L"permitted") < message.size())) };

        if (options().ignore_access_error && isFailToAccess)
        {
            return;
        }

        std::wstring eventName{ toString(error) };
        std::wstring errorMessage{ message };

        if (isFailToAccess)
        {
            eventName = L"Access";
            errorMessage += L" (";
            errorMessage += toString(error);
            errorMessage += L")";
        }

        printEntryEvent(L"Error", eventName, entry, errorMessage, Color::Red);

        if (WhichDir::Source == entry.which_dir)
        {
            m_srcTreeCounter.incrementByEnum(error, entry.size, isFailToAccess);
        }
        else
        {
            m_dstTreeCounter.incrementByEnum(error, entry.size, isFailToAccess);
        }
    }

    bool BaseCountersAndErrors::printAndCountStreamErrorIf(
        const InputFileStream_t & fileStream, const Error error, const Entry & entry)
    {
        if (fileStream)
        {
            return true;
        }
        else
        {
            printAndCountError(error, entry, getStreamStateString(fileStream.rdstate()));
            return false;
        }
    }

    bool BaseCountersAndErrors::printAndCountErrorCodeIf(
        const ErrorCode_t & errorCode,
        const Error errorEnum,
        const Entry & entry,
        const std::wstring & message)
    {
        if (errorCode)
        {
            printAndCountError(errorEnum, entry, toString(errorCode).append(L": ").append(message));
            return false;
        }
        else
        {
            return true;
        }
    }

    void BaseCountersAndErrors::printAndCountMismatch(
        const Mismatch mismatch, const Entry & entry, const std::wstring & message)
    {
        if (options().job == Job::Compare)
        {
            printEntryEvent(L"Mismatch", toString(mismatch), entry, message, Color::Yellow);
        }

        m_mismatchCounter.incrementByEnumAndEntry(entry, mismatch);
    }

    void BaseCountersAndErrors::countCopy(const Entry & entry)
    {
        m_copyCounter.incrementByEntry(entry);
    }

    void BaseCountersAndErrors::countRemove(const Entry & entry)
    {
        m_removeCounter.incrementByEntry(entry);
    }

    CounterResults BaseCountersAndErrors::printCounterResults()
    {
        auto printCounterSummary = [&](TreeCounter & counter) {
            const auto [fileStrings, enumStrings] = counter.makeSummaryStrings();

            const std::size_t fileLineCountLimit{ std::min(fileStrings.size(), 9_st) };
            for (std::size_t i(0); i < fileLineCountLimit; ++i)
            {
                printLineToConsoleOnly(fileStrings.at(i), counter.fileColor());
            }

            if (fileLineCountLimit < fileStrings.size())
            {
                printLineToConsoleOnly(
                    L"   (" + std::to_wstring(fileStrings.size() - fileLineCountLimit) +
                    L" unlisted)");
            }

            for (const std::wstring & str : fileStrings)
            {
                printLineToLogfileOnly(str);
            }

            for (const std::wstring & str : enumStrings)
            {
                printLine(str, counter.enumColor());
            }
        };

        if (options().job == Job::Compare)
        {
            printCounterSummary(m_srcTreeCounter);
            printCounterSummary(m_dstTreeCounter);
            printCounterSummary(m_mismatchCounter);
        }

        printCounterSummary(m_copyCounter);
        printCounterSummary(m_removeCounter);

        return { (!m_srcTreeCounter.isEnumEmpty() || !m_dstTreeCounter.isEnumEmpty()),
                 !m_mismatchCounter.isEmpty(),
                 !m_copyCounter.isEmpty(),
                 !m_removeCounter.isEmpty() };
    }

} // namespace backup
