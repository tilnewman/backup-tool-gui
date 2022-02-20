#ifndef BACKUP_UTIL_HPP_INCLUDED
#define BACKUP_UTIL_HPP_INCLUDED
//
// util.hpp
//
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

//

constexpr std::size_t operator"" _st(unsigned long long number)
{
    return static_cast<std::size_t>(number);
}

//

namespace backup
{

    // need this because the code that throws will already have printed the error message in color
    struct silent_runtime_error : public std::runtime_error
    {
        silent_runtime_error()
            : runtime_error("")
        {}
    };

    struct keypress_caused_abort : public std::runtime_error
    {
        keypress_caused_abort(const std::string & message)
            : runtime_error(message)
        {}
    };

    // platform detection stuff

#if (defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64) || defined(__WINDOWS__))
    constexpr bool is_running_on_windows = true;
#else
    constexpr bool is_running_on_windows = false;
#endif

    // time stuff

    using Clock_t = std::chrono::high_resolution_clock;

    [[nodiscard]] inline std::size_t
        elapsedCountMs(const Clock_t::time_point & from, const Clock_t::time_point & to)
    {
        using namespace std::chrono;
        return static_cast<std::size_t>(duration_cast<milliseconds>(to - from).count());
    }

    [[nodiscard]] inline std::size_t elapsedCountMs(const Clock_t::time_point & from)
    {
        return elapsedCountMs(from, Clock_t::now());
    }

    [[nodiscard]] inline std::wstring prettyTimeDurationString(const Clock_t::duration & dur)
    {
        using namespace std::chrono;

        if (const auto ns{ duration_cast<nanoseconds>(dur).count() }; ns < 1000)
        {
            return (std::to_wstring(ns) + L"ns");
        }

        if (const auto ms{ duration_cast<milliseconds>(dur).count() }; ms < 1000)
        {
            return (std::to_wstring(ms) + L"ms");
        }

        if (const auto secf{ duration<double>(dur).count() }; secf < 10.0)
        {
            std::wostringstream ss;
            ss << std::fixed << std::setprecision(1) << secf << L"s";
            return ss.str();
        }

        if (const auto sec{ duration_cast<seconds>(dur).count() }; sec < 60)
        {
            return (std::to_wstring(sec) + L"s");
        }

        const auto sec{ duration_cast<seconds>(dur).count() % 60 };
        const auto min{ duration_cast<minutes>(dur).count() % 60 };
        const auto hrs{ duration_cast<hours>(dur).count() };

        std::wostringstream ss;

        if (hrs > 0)
        {
            ss << std::setfill(L'0') << hrs << L':';
        }

        if (0 == min)
        {
            if (hrs > 0)
            {
                ss << L"00:";
            }
        }
        else
        {
            if (hrs > 0)
            {
                ss << std::setfill(L'0') << std::setw(2);
            }
            else
            {
                ss << std::setw(0);
            }

            ss << min << L':';
        }

        ss << std::setw(2) << std::setfill(L'0') << sec;
        return ss.str();
    }

    [[nodiscard]] inline std::wstring prettyTimeDurationString(const Clock_t::time_point & from)
    {
        return prettyTimeDurationString(Clock_t::now() - from);
    }

    // percent stuff

    template <typename T, typename U = T, typename Return_t = T>
    [[nodiscard]] Return_t calcPercent(const T numerator, const U denominator)
    {
        static_assert(std::is_integral_v<T>);
        static_assert(!std::is_same_v<std::remove_cv_t<T>, bool>);

        static_assert(std::is_integral_v<U>);
        static_assert(!std::is_same_v<std::remove_cv_t<U>, bool>);

        Return_t result{ 0 };

        if (denominator > 0)
        {
            result = static_cast<Return_t>(
                (static_cast<long double>(numerator) / static_cast<long double>(denominator)) *
                100.0L);
        }

        return result;
    }

    template <typename T, typename U = T, typename Return_t = T>
    [[nodiscard]] std::wstring calcPercentString(const T numerator, const U denominator)
    {
        return (std::to_wstring(calcPercent<T, U, Return_t>(numerator, denominator)) + L"%");
    }

} // namespace backup

#endif // BACKUP_UTIL_HPP_INCLUDED
