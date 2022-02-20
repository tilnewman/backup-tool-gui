#ifndef BACKUP_VERIFIED_OUTPUT_HPP_INCLUDED
#define BACKUP_VERIFIED_OUTPUT_HPP_INCLUDED
//
// verified-output.hpp
//
#include "enums.hpp"
#include "filesystem-common.hpp"
#include "util.hpp"

#include <fstream>
#include <iosfwd>
#include <mutex>
#include <string>

namespace backup
{

    // This is basically a wrapper for wcout that checks wcout.rdstate() after each use and resets
    // if needed. I had to resort to this because both the Windows and Linux filesystems support all
    // kinds of strange unicode characters that will cause errors -even in the "wide" wcout.  These
    // errrs don't throw exceptions, but they do prevent all further uses of wcout from doing
    // anything.
    class VerifiedOutput
    {
      public:
        VerifiedOutput(const std::wstring & logFilename = L"");

        void color(const bool willEnable);
        bool color() const;

        void print(std::wstring_view sv, const Color color = Color::Default);
        void printToConsoleOnly(std::wstring_view sv, const Color color = Color::Default);
        void printToLogfileOnly(std::wstring_view sv);

        Clock_t::time_point lastPrintTime() const;

      private:
        inline bool canWriteToLogfile() const
        {
            return (m_logFileStream.is_open() && m_logFileStream.good());
        }

        bool setupLogfile(const std::wstring & logFilename);

        void print_internal(std::wstring_view sv, const Color color = Color::Default);

        void printTo_internal(
            std::wostream & os, std::wstring_view sv, const Color color = Color::Default);

        void colorStart(std::wostream & os, const Color color) const;
        void colorStop(std::wostream & os) const;
        void alertColorSwitch(std::wostream & os, const Color color) const;
        void alertColorRestore(std::wostream & os, const Color color) const;

      private:
        bool m_isColorAllowed;
        OutputFileStream_t m_logFileStream;
        Clock_t::time_point m_lastPrintTime;

        // this is only locked by the public functions, so as long as they all only call private
        // functions this will work fine without being a recursive_mutex
        mutable std::mutex m_publicFunctionMutex;
    };

} // namespace backup

#endif // BACKUP_VERIFIED_OUTPUT_HPP_INCLUDED
