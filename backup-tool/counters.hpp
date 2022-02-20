#ifndef BACKUP_COUNTERS_HPP_INCLUDED
#define BACKUP_COUNTERS_HPP_INCLUDED
//
// counter.hpp
//
#include "entry.hpp"
#include "enums.hpp"
#include "filesystem-common.hpp"
#include "util.hpp"
#include "verified-output.hpp"

#include <array>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace backup
{

    namespace counting
    {

        struct Counted
        {
            std::wstring name;
            std::size_t number = 0;
            std::size_t count  = 0;
            std::size_t bytes  = 0;
        };

        using CountedVec_t     = std::vector<Counted>;
        using CountedVecIter_t = CountedVec_t::iterator;

        //

        struct CountStrings
        {
            CountStrings() = default;

            CountStrings(
                const Counted & ct, const std::size_t totalCount, const std::size_t totalBytes);

            std::wstring name;
            std::wstring count;
            std::wstring count_percent;
            std::wstring size;
            std::wstring size_percent;
        };

        using CountStrVec_t = std::vector<CountStrings>;

        //

        class Counter
        {
          public:
            Counter();

            std::size_t totalCount() const;
            std::size_t totalByteCount() const;
            std::size_t totalUniqueNumbers() const;
            std::size_t totalUniqueNames() const;

            bool isEmpty() const noexcept { return m_counteds.empty(); }

            void incrementByName(const std::wstring & name, const std::size_t size);

            void incrementByNumber(
                const std::size_t number, const std::wstring & name, const std::size_t size);

            template <typename T>
            void incrementByEnum(const T & enumeration, const std::size_t size)
            {
                incrementByNumber(
                    static_cast<std::size_t>(enumeration), toString(enumeration), size);
            }

            std::vector<std::wstring> makeSummaryStrings(const std::size_t lineCountLimit);

          private:
            CountStrVec_t makeLineStringVecs(const std::size_t lineCountLimit) const;
            void justifyLineStrings(CountStrVec_t & lines) const;

          private:
            CountedVec_t m_counteds;
        };

    } // namespace counting

    //

    class TreeCounter
    {
      public:
        TreeCounter(
            const std::wstring & title,
            const Color fileColor          = Color::Default,
            const std::wstring & enumTitle = L"",
            const Color enumColor          = Color::Default);

        inline Color fileColor() const noexcept { return m_fileColor; }
        inline Color enumColor() const noexcept { return m_enumColor; }

        inline bool isCountEmpty() const
        {
            std::scoped_lock scopedLock(m_mutex);
            return (m_fileExtensionCounter.totalCount() == 0);
        }

        inline bool isEnumEmpty() const
        {
            std::scoped_lock scopedLock(m_mutex);
            return (m_enumCounter.totalCount() == 0);
        }

        inline bool isEmpty() const
        {
            std::scoped_lock scopedLock(m_mutex);
            return (
                (m_fileExtensionCounter.totalCount() == 0) && (m_enumCounter.totalCount() == 0));
        }

        template <typename T>
        void incrementByEnum(
            const T & enumeration, const std::size_t size, const bool isAccessError = false)
        {
            std::scoped_lock scopedLock(m_mutex);
            m_byteCount += size;

            if (isAccessError)
            {
                ++m_accessErrorCount;
            }

            return m_enumCounter.incrementByEnum(enumeration, size);
        }

        template <typename T>
        void incrementByEnumAndEntry(
            const Entry & entry, const T & enumeration, const bool isAccessError = false)
        {
            incrementByEnum(enumeration, entry.size, isAccessError);
            incrementByEntry(entry);

            // both the function calls above will add to m_byteCount so undo one of them
            std::scoped_lock scopedLock(m_mutex);
            m_byteCount -= entry.size;
        }

        void incrementByEntry(const Entry & entry);

        std::tuple<std::vector<std::wstring>, std::vector<std::wstring>> makeSummaryStrings();

      private:
        std::wstring m_fileTitle;
        Color m_fileColor;
        std::wstring m_enumTitle;
        Color m_enumColor;
        std::size_t m_fileCount;
        std::size_t m_directoryCount;
        std::size_t m_byteCount;
        std::size_t m_accessErrorCount;
        counting::Counter m_fileExtensionCounter;
        counting::Counter m_enumCounter;
        mutable std::mutex m_mutex;
    };

} // namespace backup

#endif // BACKUP_COUNTERS_HPP_INCLUDED
