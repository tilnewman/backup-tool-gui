#ifndef BACKUP_STR_UTIL_HPP_INCLUDED
#define BACKUP_STR_UTIL_HPP_INCLUDED
//
// str-util.hpp
//
#include "util.hpp"

#include <algorithm>
#include <codecvt>
#include <string>

namespace strutil
{

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

    [[nodiscard]] inline std::wstring toWideString(std::string_view str)
    {
        std::wstring result;

        if (!str.empty())
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
            result = converter.from_bytes(str.data(), (str.data() + str.size()));
        }

        return result;
    }

    [[nodiscard]] inline std::string toNarrowString(std::wstring_view str)
    {
        std::string result;

        if (!str.empty())
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
            result = converter.to_bytes(str.data(), (str.data() + str.size()));
        }

        return result;
    }

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    // single character query functions

    [[nodiscard]] constexpr bool isUpper(const char ch) noexcept
    {
        return ((ch >= 'A') && (ch <= 'Z'));
    }

    [[nodiscard]] constexpr bool isLower(const char ch) noexcept
    {
        return ((ch >= 'a') && (ch <= 'z'));
    }

    [[nodiscard]] constexpr bool isEitherNewline(const char ch) noexcept
    {
        return ((ch == '\r') || (ch == '\n'));
    }

    [[nodiscard]] constexpr bool isAlpha(const char ch) noexcept
    {
        return (isUpper(ch) || isLower(ch));
    }

    [[nodiscard]] constexpr bool isDigit(const char ch) noexcept
    {
        return ((ch >= '0') && (ch <= '9'));
    }

    [[nodiscard]] constexpr bool isAlphaOrDigit(const char ch) noexcept
    {
        return (isAlpha(ch) || isDigit(ch));
    }

    [[nodiscard]] constexpr bool isWhitespace(const char ch) noexcept
    {
        return ((ch == ' ') || (ch == '\t'));
    }

    [[nodiscard]] constexpr bool isDisplayable(const char ch) noexcept
    {
        return ((ch >= ' ') && (ch != 127));
    }

    // single character case changing functions

    [[nodiscard]] constexpr char toUpperCopy(const char ch) noexcept
    {
        if (isLower(ch))
        {
            return static_cast<char>(ch - 32); //-V112
        }
        else
        {
            return ch;
        }
    }

    constexpr void toUpper(char & ch) noexcept { ch = toUpperCopy(ch); }

    [[nodiscard]] constexpr char toLowerCopy(const char ch) noexcept
    {
        if (isUpper(ch))
        {
            return static_cast<char>(ch + 32); //-V112
        }
        else
        {
            return ch;
        }
    }

    constexpr void toLower(char & ch) noexcept { ch = toLowerCopy(ch); }

    [[nodiscard]] constexpr char flipCaseCopy(const char ch) noexcept
    {
        if (isLower(ch))
        {
            return toUpperCopy(ch);
        }
        else if (isUpper(ch))
        {
            return toLowerCopy(ch);
        }
        else
        {
            return ch;
        }
    }

    constexpr void flipCase(char & ch) noexcept { ch = flipCaseCopy(ch); }

    // whole string case changing functions

    inline void toUpper(std::string & str) noexcept
    {
        for (char & ch : str)
        {
            toUpper(ch);
        }
    }

    [[nodiscard]] inline std::string toUpperCopy(const std::string & orig) noexcept
    {
        std::string copy{ orig };
        toUpper(copy);
        return copy;
    }

    inline void toLower(std::string & str) noexcept
    {
        for (char & ch : str)
        {
            toLower(ch);
        }
    }

    [[nodiscard]] inline std::string toLowerCopy(const std::string & orig) noexcept
    {
        std::string copy{ orig };
        toLower(copy);
        return copy;
    }

    inline void flipCase(std::string & str) noexcept
    {
        for (char & ch : str)
        {
            flipCase(ch);
        }
    }

    [[nodiscard]] inline std::string flipCaseCopy(const std::string & orig) noexcept
    {
        std::string copy{ orig };
        flipCase(copy);
        return copy;
    }

    // trim  functions

    template <typename WillTrimDetectorLambda_t>
    void trimIf(std::string & str, WillTrimDetectorLambda_t willTrimDetectorLambda)
    {
        str.erase(
            std::begin(str),
            std::find_if_not(std::cbegin(str), std::cend(str), willTrimDetectorLambda));

        str.erase(
            std::find_if_not(std::crbegin(str), std::crend(str), willTrimDetectorLambda).base(),
            std::end(str));
    }

    template <typename WillTrimDetectorLambda_t>
    [[nodiscard]] std::string
        trimIfCopy(const std::string & orig, WillTrimDetectorLambda_t willTrimDetectorLambda)
    {
        std::string copy{ orig };
        trimIf(copy, willTrimDetectorLambda);
        return copy;
    }

    inline void trimWhitespace(std::string & str) { trimIf(str, isWhitespace); }

    [[nodiscard]] inline std::string trimWhitespaceCopy(const std::string & orig)
    {
        return trimIfCopy(orig, isWhitespace);
    }

} // namespace strutil

#endif // BACKUP_STR_UTIL_HPP_INCLUDED
