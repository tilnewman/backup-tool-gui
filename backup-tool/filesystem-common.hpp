#ifndef BACKUP_FILESYSTEM_COMMON_HPP_INCLUDED
#define BACKUP_FILESYSTEM_COMMON_HPP_INCLUDED
//
// filesystem-common.hpp
//  Some don't support the new c++17 <filesystem> library so this is a work-around.
//
#include "dir-pair.hpp"
#include "str-util.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

//

//
// Links  (i.e. symlinks/shortcuts/junctions/etc)
//  - This app never follows links of any kind to what they point to.
//  - All non-directories are treated as regular flat files, or a type error is reported.
//  - So linux symlinks and windows shortcuts are supported because they can be treated as flat
//    files, but Windows symlinks and junctions are NOT supported because they cannot be treated
//    as regular flat file.
//  - Final file/dir counts/sizes will be different if there are links, but only when the app is
//    run from different operating systems, so who cares.
//  - Windows also supports symlinks in addition to junctions and shortcuts. These Windows
//    symlinks can point to anything, and both explorer and the command line CAN use them.  I've
//    just never known anyone who knows this and uses them, so who cares.
//  - Windows junctions are always to directories, but explorer and the command-line cannot use
//    them.  The std::filesytem lib will correctly identify them as junctions, but boost
//    filesystem will show them as symlinks.
//
// On Windows:
//  Linux symlinks:       SUPPORTED       as files with valid sizes >zero.
//  Windows shortcuts     SUPPORTED       as files with valid sizes >zero.
//  Windows symlinks:     NOT SUPPORTED   because Windows can't handle them as flat files.
//  Windows junctions:    NOT SUPPORTED   because:
//      std::filesystem:        sees them with valid sizes >0, but not as files, dirs, or symlinks
//                              that it can copy as this app needs to.
//      boost::filesystem:      sees them as symlinks, but errors when trying to find their size.
//
// On Linux/MacOS:
//  Linux symlinks:       SUPPORTED     as files with valid sizes =zero.
//  Windows shortcuts:    SUPPORTED     BUT NOT as flat files.  Win symlinks to dirs look normal!
//  Windows junctions:    SUPPORTED     BUT NOT as flat files.  Win junctions look normal!
//
// So the differences are:
//    (1) Windows and Linux/MacOS/other handle shortcuts/symlinks/junctions differently, and even
//        see them having different sizes.
//
//    (2) Windows Junctions (i.e. "C:\Documents and Settings" and "C:\Users\Default User") are
//        supported in Linux/MacOS, but (ironically) not in Windows.
//

namespace backup
{

    namespace fs = std::filesystem;

    using ErrorCode_t        = std::error_code;
    using InputFileStream_t  = std::ifstream;
    using OutputFileStream_t = std::wofstream;

    inline std::size_t getSizeCommon(const fs::directory_entry & dirEntry, ErrorCode_t & errorCode)
    {
        return dirEntry.file_size(errorCode);
    }

    inline void copyFileCommon(const fs::path & from, const fs::path & to, ErrorCode_t & errorCode)
    {
        fs::copy(from, to, fs::copy_options::copy_symlinks, errorCode);
    }

    [[nodiscard]] constexpr auto toString(const fs::file_type fileType) noexcept
    {
        // clang-format off
        switch (fileType)
        {
            case fs::file_type::none:       { return L"none"; }
            case fs::file_type::not_found:  { return L"not_found"; }
            case fs::file_type::regular:    { return L"file"; }
            case fs::file_type::directory:  { return L"directory"; }
            case fs::file_type::symlink:    { return L"symlink"; }
            case fs::file_type::block:      { return L"block"; }
            case fs::file_type::character:  { return L"character"; }
            case fs::file_type::fifo:       { return L"fifo"; }
            case fs::file_type::socket:     { return L"socket"; }
            case fs::file_type::unknown:    { return L"unknown"; }
            case fs::file_type::junction:   { return L"junction"; }
            default:                        { return L"(UNKNOWN_FILE_TYPE_ENUM_ERROR)"; }
        }
        // clang-format on
    }

    [[nodiscard]] inline std::wstring toString(const ErrorCode_t & errorCode)
    {
        std::string str;
        str += "error_code=";
        str += std::to_string(errorCode.value());
        str += '=';
        str += errorCode.category().name();
        str += "=\"";
        str += errorCode.message();
        str += '\"';
        return strutil::toWideString(str);
    }

    template <typename T>
    [[nodiscard]] std::wstring getStreamStateString(const T state) noexcept
    {
        std::wstring str;
        str.reserve(256);

        // clang-format off
        if (state == std::ios::goodbit) { str += L"good/"; }
        if (state == std::ios::eofbit)  { str += L"end_of_file/"; }
        if (state == std::ios::failbit) { str += L"fformat_or_extract_error/"; }
        if (state == std::ios::badbit)  { str += L"irrecoverable_stream_error/"; }
        // clang-format on

        if (str.empty())
        {
            str += L"unknown_error";
        }

        if (str.back() == L'/')
        {
            str.pop_back();
        }

        return (L"fstream_" + str);
    }

    [[nodiscard]] inline std::wstring fileSizeToString(const std::size_t size)
    {
        std::wostringstream ss;
        ss.imbue(std::locale("")); // this puts commas in big numbers

        auto appendSize =
            [&](const std::size_t step, const wchar_t letter, const bool willForce = false) {
                if ((size < step) || willForce)
                {
                    ss << std::setprecision(3)
                       << (static_cast<long double>(size) / static_cast<long double>(step / 1000))
                       << letter;

                    return true;
                }
                else
                {
                    return false;
                }
            };

        if (!appendSize(1000, L'B'))
        {
            if (!appendSize(1000'000, L'K'))
            {
                if (!appendSize(1'000'000'000, L'M'))
                {
                    appendSize(1'000'000'000'000, L'G', true);
                }
            }
        }

        return ss.str();
    }

    [[nodiscard]] constexpr bool isDirectorySeparator(const wchar_t ch) noexcept
    {
        return ((ch == fs::path::preferred_separator) || (ch == L'\\') || (ch == L'/'));
    }

    [[nodiscard]] inline bool existsIgnoringErrors(const fs::path & path, const bool returnOnError)
    {
        try
        {
            ErrorCode_t errorCodeIgnored;
            const bool result{ fs::exists(path, errorCodeIgnored) };

            if (errorCodeIgnored)
            {
                return returnOnError;
            }
            else
            {
                return result;
            }
        }
        catch (...)
        {
            return returnOnError;
        }
    }

} // namespace backup

#endif // BACKUP_FILESYSTEM_COMMON_HPP_INCLUDED
