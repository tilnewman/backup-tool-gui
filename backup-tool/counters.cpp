// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
//
// counters.cpp
//
#include "counters.hpp"

#include "str-util.hpp"
#include "verified-output.hpp"

#include <cassert>
#include <set>

namespace backup
{
    namespace counting
    {

        CountStrings::CountStrings(
            const Counted & ct, const std::size_t totalCount, const std::size_t totalBytes)
            : name(ct.name)
            , count(std::to_wstring(ct.count))
            , count_percent(calcPercentString(ct.count, totalCount))
            , size((ct.bytes == 0) ? L"" : fileSizeToString(ct.bytes))
            , size_percent((ct.bytes == totalBytes) ? L"" : calcPercentString(ct.bytes, totalBytes))
        {}

        Counter::Counter()
            : m_counteds()
        {
            m_counteds.reserve(1024);
        }

        std::size_t Counter::totalCount() const
        {
            return std::accumulate(
                std::begin(m_counteds),
                std::end(m_counteds),
                0_st,
                [](const std::size_t sumSoFar, const Counted & ct) {
                    return (sumSoFar + ct.count);
                });
        }

        std::size_t Counter::totalByteCount() const
        {
            return std::accumulate(
                std::begin(m_counteds),
                std::end(m_counteds),
                0_st,
                [](const std::size_t sumSoFar, const Counted & ct) {
                    return (sumSoFar + ct.bytes);
                });
        }

        std::size_t Counter::totalUniqueNumbers() const
        {
            std::set<std::size_t> uniqueNumbers;

            std::transform(
                std::begin(m_counteds),
                std::end(m_counteds),
                std::inserter(uniqueNumbers, std::end(uniqueNumbers)),
                [](const Counted & ct) { return ct.number; });

            return uniqueNumbers.size();
        }

        std::size_t Counter::totalUniqueNames() const
        {
            std::set<std::wstring> uniqueNames;

            std::transform(
                std::begin(m_counteds),
                std::end(m_counteds),
                std::inserter(uniqueNames, std::end(uniqueNames)),
                [](const Counted & ct) { return ct.name; });

            return uniqueNames.size();
        }

        void Counter::incrementByName(const std::wstring & name, const std::size_t size)
        {
            const auto iter{ std::find_if(
                std::begin(m_counteds), std::end(m_counteds), [&name](const Counted & ct) {
                    return (ct.name == name);
                }) };

            if (iter == std::end(m_counteds))
            {
                m_counteds.push_back(Counted{ name, 0_st, 1_st, size });
            }
            else
            {
                iter->count++;
                iter->bytes += size;
            }
        }

        void Counter::incrementByNumber(
            const std::size_t number, const std::wstring & name, const std::size_t size)
        {
            const std::size_t containerSize{ m_counteds.size() };

            if (number >= containerSize)
            {
                m_counteds.resize(20 + (number * 2));
            }

            Counted & ct{ m_counteds[number] };

            if (ct.name.empty())
            {
                ct.name = name;
            }

            ct.number = number;
            ++ct.count;
            ct.bytes += size;
        }

        std::vector<std::wstring> Counter::makeSummaryStrings(const std::size_t lineCountLimit)
        {
            std::vector<std::wstring> allStrings;

            m_counteds.erase(
                std::remove_if(
                    std::begin(m_counteds),
                    std::end(m_counteds),
                    [](const auto & ct) {
                        return (
                            ct.name.empty() && (ct.number == 0) && (ct.count == 0) &&
                            (ct.bytes == 0));
                    }),
                std::end(m_counteds));

            if (m_counteds.empty())
            {
                return allStrings;
            }

            std::sort(
                std::begin(m_counteds), std::end(m_counteds), [](const auto & A, const auto & B) {
                    if (A.count != B.count)
                    {
                        return (A.count > B.count);
                    }
                    else if (A.bytes != B.bytes)
                    {
                        return (A.bytes > B.bytes);
                    }
                    else if (A.name != B.name)
                    {
                        return (A.name > B.name);
                    }
                    else
                    {
                        return (A.number > B.number);
                    }
                });

            CountStrVec_t lines{ makeLineStringVecs(lineCountLimit) };
            justifyLineStrings(lines);

            for (const auto & strings : lines)
            {
                std::wstring str;

                str += L"   ";
                str += strings.name;
                str += L" -  ";
                str += strings.count;
                str += L"x ";
                str += strings.count_percent;

                if (!strings.size.empty())
                {
                    str += L"  - ";
                    str += strings.size;

                    if (!strings.size_percent.empty())
                    {
                        str += L" ";
                        str += strings.size_percent;
                    }
                }

                allStrings.push_back(str);
            }

            return allStrings;
        }

        CountStrVec_t Counter::makeLineStringVecs(const std::size_t lineCountLimit) const
        {
            CountStrVec_t lines;

            const std::size_t allCount{ totalCount() };
            const std::size_t allBytes{ totalByteCount() };
            const std::size_t containerSize{ m_counteds.size() };

            const std::size_t linesToDisplayCount{ (lineCountLimit == 0)
                                                       ? containerSize
                                                       : std::min(lineCountLimit, containerSize) };

            std::size_t i(0);
            for (; i < linesToDisplayCount; ++i)
            {
                lines.emplace_back(m_counteds.at(i), allCount, allBytes);
            }

            if (i < containerSize)
            {
                std::size_t notListedCount{ 0 };
                std::size_t notListedTotalCount{ 0 };
                std::size_t notListedTotalSize{ 0 };

                for (; i < containerSize; ++i)
                {
                    const auto & counted{ m_counteds.at(i) };
                    ++notListedCount;
                    notListedTotalCount += counted.count;
                    notListedTotalSize += counted.bytes;
                }

                Counted notListedCounted{
                    L"(unlisted)", 0, notListedTotalCount, notListedTotalSize
                };

                lines.emplace_back(notListedCounted, allCount, allBytes);
            }

            return lines;
        }

        void Counter::justifyLineStrings(CountStrVec_t & lines) const
        {
            std::size_t nameLengthMax{ 0 };
            std::size_t countLengthMax{ 0 };
            std::size_t sizeLengthMax{ 0 };

            for (const auto & strings : lines)
            {
                if (strings.name.length() > nameLengthMax)
                {
                    nameLengthMax = strings.name.length();
                }

                if (strings.count.length() > countLengthMax)
                {
                    countLengthMax = strings.count.length();
                }

                if (strings.size.length() > sizeLengthMax)
                {
                    sizeLengthMax = strings.size.length();
                }
            }

            enum class Justify
            {
                Left,
                Right
            };

            auto stretchAndjustify =
                [](std::wstring & str, const Justify justify, const std::size_t lengthMin) {
                    if (str.empty() || (lengthMin == 0) || (str.length() >= lengthMin))
                    {
                        return;
                    }

                    const std::size_t charsToAddCount{ lengthMin - str.length() };

                    if (justify == Justify::Left)
                    {
                        str.append(charsToAddCount, L' ');
                    }
                    else
                    {
                        str.insert(0, charsToAddCount, L' ');
                    }
                };

            for (auto & strings : lines)
            {
                stretchAndjustify(strings.name, Justify::Left, nameLengthMax);
                stretchAndjustify(strings.count, Justify::Right, countLengthMax);
                stretchAndjustify(strings.count_percent, Justify::Right, 4); //-V112
                stretchAndjustify(strings.size, Justify::Right, sizeLengthMax);
                stretchAndjustify(strings.size_percent, Justify::Right, 4); //-V112
            }
        }

    } // namespace counting

    TreeCounter::TreeCounter(
        const std::wstring & title,
        const Color fileColor,
        const std::wstring & enumTitle,
        const Color enumColor)
        : m_fileTitle(title)
        , m_fileColor(fileColor)
        , m_enumTitle(enumTitle)
        , m_enumColor(enumColor)
        , m_fileCount(0)
        , m_directoryCount(0)
        , m_byteCount(0)
        , m_accessErrorCount(0)
        , m_fileExtensionCounter()
        , m_enumCounter()
    {}

    void TreeCounter::incrementByEntry(const Entry & entry)
    {
        std::scoped_lock scopedLock(m_mutex);

        m_byteCount += entry.size;

        if (entry.is_file)
        {
            ++m_fileCount;

            m_fileExtensionCounter.incrementByName(
                ((entry.extension.empty()) ? L"\"\"" : entry.extension), entry.size);
        }
        else
        {
            ++m_directoryCount;
        }
    }

    std::tuple<std::vector<std::wstring>, std::vector<std::wstring>>
        TreeCounter::makeSummaryStrings()
    {
        std::scoped_lock scopedLock(m_mutex);

        assert(m_fileCount == m_fileExtensionCounter.totalCount());

        std::wostringstream ss;
        ss.imbue(std::locale("")); // this is only to put commas in the big numbers

        auto appendNewLine = [&ss](auto & vec) {
            vec.push_back(ss.str());
            ss.str(L"");
        };

        std::vector<std::wstring> fileStrings;

        if ((m_fileCount > 0) || (m_directoryCount > 0))
        {
            ss << m_fileTitle << L" x" << (m_fileCount + m_directoryCount);
            appendNewLine(fileStrings);

            ss << L" " << std::setw(10) << std::left << m_directoryCount << L"Directories";
            appendNewLine(fileStrings);

            ss << L" " << std::setw(10) << std::left << m_fileCount << L"Files  "
               << fileSizeToString(m_byteCount) << " (" << m_byteCount << "bytes)";

            appendNewLine(fileStrings);

            for (const std::wstring & str : m_fileExtensionCounter.makeSummaryStrings(0))
            {
                fileStrings.push_back(str);
            }
        }

        std::vector<std::wstring> enumStrings;

        if (!m_enumCounter.isEmpty())
        {
            const auto enumCountStrings{ m_enumCounter.makeSummaryStrings(0) };

            ss << m_enumTitle << L" x" << enumCountStrings.size();
            appendNewLine(enumStrings);

            for (const std::wstring & str : enumCountStrings)
            {
                enumStrings.push_back(str);
            }

            if (m_accessErrorCount > 0)
            {
                ss << L"   (Access x" << m_accessErrorCount << L")";
                appendNewLine(enumStrings);
            }
        }

        return { fileStrings, enumStrings };
    }

} // namespace backup
