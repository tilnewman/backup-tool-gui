#ifndef BACKUP_ENUMS_HPP_INCLUDED
#define BACKUP_ENUMS_HPP_INCLUDED
//
// enums.hpp
//
#include <cstddef>
#include <string>

namespace backup
{

    enum class WhichDir
    {
        Source,
        Destination
    };

    [[nodiscard]] constexpr auto toString(const WhichDir dir) noexcept
    {
        return ((WhichDir::Source == dir) ? L"Source" : L"Destination");
    }

    [[nodiscard]] constexpr auto toStringShort(const WhichDir dir) noexcept
    {
        return ((WhichDir::Source == dir) ? L"src" : L"dst");
    }

    enum class Job
    {
        Compare,
        Copy,
        Cull
    };

    [[nodiscard]] constexpr auto toString(const Job job) noexcept
    {
        // clang-format off
    switch (job)
    {
        case Job::Compare: return L"Compare";
        case Job::Copy: return L"Copy";
        case Job::Cull: return L"Cull";
        default: return L"UNKNOWN_JOB_ENUM_ERROR";
    }
        // clang-format on
    }

    enum class Error
    {
        Exists,
        Status,
        SymlinkStatus,
        Size,
        UnsupportedType,
        DirIterMake,
        DirIterInc,
        CreateDirectory,
        Open,
        Read,
        Remove,
        Copy
    };

    [[nodiscard]] constexpr auto toString(const Error error) noexcept
    {
        // clang-format off
    switch (error)
    {
        case Error::Exists:          return L"Exists";
        case Error::Status:          return L"Status";
        case Error::SymlinkStatus:   return L"SymStatus";
        case Error::Size:            return L"Size";
        case Error::Open:            return L"Open";
        case Error::Read:            return L"Read";
        case Error::DirIterMake:     return L"DirItrM";
        case Error::DirIterInc:      return L"DirItrI";
        case Error::CreateDirectory: return L"CreateDir";
        case Error::UnsupportedType: return L"Type";
        case Error::Copy:            return L"Copy";
        case Error::Remove:          return L"Delete";
        default:                     return L"UNKNOWN_ERROR_ENUM_ERROR";
    }
        // clang-format on
    }

    [[nodiscard]] constexpr bool isAccessError(const Error error) noexcept
    {
        // clang-format off
    switch (error)
    {
        case Error::Exists:
        case Error::Status:
        case Error::SymlinkStatus:
        case Error::Open:
        case Error::Read:
        case Error::Size:            return true;
        case Error::DirIterMake:     
        case Error::DirIterInc:
        case Error::CreateDirectory:
        case Error::UnsupportedType:
        case Error::Copy:
        case Error::Remove:
        default:                     return false;
    }
        // clang-format on
    }

    enum class Mismatch
    {
        Modified,
        Size,
        Extra,
        Missing
    };

    [[nodiscard]] constexpr auto toString(const Mismatch mismatch) noexcept
    {
        // clang-format off
    switch (mismatch)
    {
        case Mismatch::Missing:  return L"Missing";
        case Mismatch::Extra:    return L"Extra";
        case Mismatch::Size:     return L"Size";
        case Mismatch::Modified: return L"Modified";
        default:                 return L"UNKNOWN_MISMATCH_ENUM_ERROR";
    }
        // clang-format on
    }

    enum class Color
    {
        Default,
        Gray,
        Green,
        Yellow,
        Red,
        Disabled
    };

    [[nodiscard]] constexpr auto toConsoleCode(const Color color) noexcept
    {
        // clang-format off
    switch (color)
    {
        case Color::Default:    return L"\033[0;0m";
        case Color::Gray:       return L"\033[37;40m";
        case Color::Green:      return L"\033[32;40m";
        case Color::Yellow:     return L"\033[33;40m";
        case Color::Red:        return L"\033[91;40m";
        case Color::Disabled:
        default:                return L"";
    }
        // clang-format on
    }

} // namespace backup

#endif // BACKUP_ENUMS_HPP_INCLUDED
