// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
//
// verified-output.cpp
//
#include "verified-output.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

#pragma warning(disable : 4996) // allow use of old unsafe localtime()

namespace backup
{

    VerifiedOutput::VerifiedOutput(const std::wstring & logFilename)
        : m_isColorAllowed(false)
        , m_logFileStream()
        , m_lastPrintTime()
        , m_publicFunctionMutex()
    {
        setupLogfile(logFilename);
    }

    void VerifiedOutput::color(const bool willEnable)
    {
        std::scoped_lock scopedLock(m_publicFunctionMutex);
        m_isColorAllowed = willEnable;
    }

    bool VerifiedOutput::color() const
    {
        std::scoped_lock scopedLock(m_publicFunctionMutex);
        return m_isColorAllowed;
    }

    void VerifiedOutput::print(std::wstring_view sv, const Color color)
    {
        if (sv.empty())
        {
            return;
        }

        std::scoped_lock scopedLock(m_publicFunctionMutex);
        print_internal(sv, color);
    }

    void VerifiedOutput::printToConsoleOnly(std::wstring_view sv, const Color color)
    {
        std::scoped_lock scopedLock(m_publicFunctionMutex);
        printTo_internal(std::wcout, sv, color);
    }

    void VerifiedOutput::printToLogfileOnly(std::wstring_view sv)
    {
        std::scoped_lock scopedLock(m_publicFunctionMutex);

        if (canWriteToLogfile())
        {
            printTo_internal(m_logFileStream, sv, Color::Disabled);
        }
    }

    Clock_t::time_point VerifiedOutput::lastPrintTime() const
    {
        std::scoped_lock scopedLock(m_publicFunctionMutex);
        return m_lastPrintTime;
    }

    bool VerifiedOutput::setupLogfile(const std::wstring & logFilenameBase)
    {
        if (logFilenameBase.empty())
        {
            return false;
        }

        ErrorCode_t errorCode;
        fs::path path{ fs::current_path(errorCode) }; //-V821
        if (errorCode)
        {
            print_internal(
                L"Path Error: Unable to establish the current path when trying to make the "
                L"logfile. "
                L"{std::filesystem::current_path() error=\"" +
                    toString(errorCode) + L"\"}",
                Color::Red);

            return false;
        }

        {
            using namespace std::chrono;
            const time_t nowCTime{ system_clock::to_time_t(system_clock::now()) };

            std::ostringstream timeSS;
            timeSS << std::put_time(localtime(&nowCTime), "--%F--%H-%M-%S--");
            const std::wstring timeStr{ strutil::toWideString(timeSS.str()) };

            const std::size_t maxDigitCount{ 3 };
            std::size_t fileNumber{ 0 };
            std::wstring finalFilenameStr;
            do
            {
                std::wstring fileNumberStr = std::to_wstring(fileNumber++);
                if (fileNumberStr.size() < maxDigitCount)
                {
                    fileNumberStr = std::wstring((maxDigitCount - fileNumberStr.size()), L'0')
                                        .append(fileNumberStr);
                }

                finalFilenameStr = (logFilenameBase + timeStr + fileNumberStr + L".log");

            } while (fs::exists((path / finalFilenameStr), errorCode));

            path /= finalFilenameStr;
        }

        if (m_logFileStream.is_open())
        {
            m_logFileStream.close();
            m_logFileStream.clear();
        }

        m_logFileStream.open(path, (std::ios::trunc | std::ios::out));

        if (!canWriteToLogfile())
        {
            print_internal(
                std::wstring(L"Error: ") + getStreamStateString(m_logFileStream.rdstate()) +
                    L": While trying to create/truncate the logfile: \"" + path.wstring() + L"\"",
                Color::Red);

            return false;
        }

        return true;
    }

    void VerifiedOutput::print_internal(std::wstring_view sv, const Color color)
    {
        printTo_internal(std::wcout, sv, color);

        if (canWriteToLogfile())
        {
            printTo_internal(m_logFileStream, sv, Color::Disabled);
        }
    }

    void VerifiedOutput::printTo_internal(
        std::wostream & os, std::wstring_view sv, const Color color)
    {
        std::size_t badCharacterCount{ 0 };

        if (m_isColorAllowed && (color != Color::Disabled))
        {
            colorStart(os, color);
        }

        for (std::size_t i(0); i < sv.size(); ++i)
        {
            bool wasException{ false };

            try
            {
                os << sv[i];
            }
            catch (...)
            {
                wasException = true;
            }

            if (wasException || !os.good())
            {
                os.clear();
                os.flush();

                alertColorSwitch(os, color);
                os << L"?";
                alertColorRestore(os, color);

                ++badCharacterCount;
            }
        }

        if (badCharacterCount > 0)
        {
            alertColorSwitch(os, color);
            os << "   {output_error_" << badCharacterCount << "_bad_chars}";
            alertColorRestore(os, color);
        }

        if (m_isColorAllowed && (color != Color::Disabled))
        {
            colorStop(os);
        }

        os << std::endl;

        m_lastPrintTime = Clock_t::now();
    }

    void VerifiedOutput::colorStart(std::wostream & os, const Color color) const
    {
        if (!m_isColorAllowed || (color == Color::Disabled))
        {
            return;
        }

        os << toConsoleCode(color);
    }

    void VerifiedOutput::colorStop(std::wostream & os) const { colorStart(os, Color::Default); }

    void VerifiedOutput::alertColorSwitch(std::wostream & os, const Color color) const
    {
        if (!m_isColorAllowed || (color == Color::Disabled))
        {
            return;
        }

        if (Color::Yellow == color)
        {
            colorStart(os, Color::Red);
        }
        else
        {
            colorStart(os, Color::Yellow);
        }
    }

    void VerifiedOutput::alertColorRestore(std::wostream & os, const Color color) const
    {
        if (!m_isColorAllowed || (color == Color::Disabled))
        {
            return;
        }

        colorStart(os, color);
    }

} // namespace backup
