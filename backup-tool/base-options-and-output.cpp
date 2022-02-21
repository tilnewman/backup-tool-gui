// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
//
// base-options-and-output.cpp
//
#include "base-options-and-output.hpp"

#include "str-util.hpp"
#include "util.hpp"

#include <algorithm>
#include <cassert>
#include <thread>

namespace backup
{

    BaseOptionsAndOutput::BaseOptionsAndOutput(const std::vector<std::string> & args)
        : m_options()
        , m_output(L"backup")
    {
        setOptions(args);

        printJobSummary(args);
        printConflictingOptionsWarnings();
        printOptionsSummary();
    }

    void BaseOptionsAndOutput::printUsage()
    {
        std::wostringstream ss;

        ss << L"\nUsage:\n";
        ss << L"   backup <options> <source_dir> <destination_dir>\n";
        ss << L"    -";
        printLine(ss.str());

        printLine(
            L"    Note: This app checks every bit of every file, but ignores all dates/times.",
            Color::Yellow);

        // clang-format off
    ss.str(L"");
    ss << L"    -\n";
    ss << L"    --compare         Shows all missing/modified/extra files/dirs, but does nothing.\n";
    ss << L"    --copy            Copies (replaces) all missing/modified files/dirs.\n";
    ss << L"    --cull            Deletes only the extra files/dirs. (anything not in src)\n";
    ss << L"    -\n";
    ss << L"    --help            Shows this, but does nothing else.\n";
    ss << L"    --dry-run         A safe mode that does nothing except show what WOULD have been done.\n";
    ss << L"    --background      Runs minimal threads to prevent slowing your computer down.\n";
    ss << L"    --skip-file-read  Files with the exact same size are assumed to have the same contents.\n";
    ss << L"    --show-relative   Displays relative paths instead of absolute paths.\n";
    ss << L"    --verbose         Shows extra info. (i.e. warns on symlinks/shortcuts/weird stuff).\n";
    ss << L"    --quiet           Shows only errors and the final result.\n";
    ss << L"    -\n";
    ss << L"    --ignore-extra    Any extra files or dirs in your dst dir                     -are not shown.\n";
    ss << L"    --ignore-access   Errors caused by access/permissions/authentication problems -are not shown.\n";
    ss << L"    --ignore-unknown  Errors caused by files or dirs with unknown types           -are not shown.\n";
    ss << L"    --ignore-warnings Warnings about unusual counts or possible errors            -are not shown..\n";
    ss << L"    --ignore-all      Same as all the ignore options above at once.\n";
        // clang-format on

        ss << L"    --color-on        Enables colored console output.";
        if (Options::isColorEnabledByDefault())
        {
            ss << L"  (default)";
        }
        ss << L"\n";

        ss << L"    --color-off       Disables colored console output.";
        if (!Options::isColorEnabledByDefault())
        {
            ss << L" (default)";
        }

        printLine(ss.str());
    }

    void BaseOptionsAndOutput::printJobSummary(const std::vector<std::string> & args)
    {
        std::wostringstream ss;

        // put the whole call with all the command line arguments in the logfile
        ss << "backup";
        for (const auto & arg : args)
        {
            ss << L" " << strutil::toWideString(arg);
        }
        ss << L"\n";

        // print the job
        if (Job::Compare == m_options.job)
        {
            ss << L"Comparing";
        }
        else if (Job::Copy == m_options.job)
        {
            ss << L"Copying";
        }
        else
        {
            ss << L"Culling";
        }

        ss << L"...\n";

        ss << L"   src: " << m_options.path_str_dpair.src;
        ss << L"\n   dst: " << m_options.path_str_dpair.dst;

        printLine(ss.str());
    }

    void BaseOptionsAndOutput::printOptionsSummary()
    {
        std::wstring str;

        auto appendFlagIf = [&](const bool is, const std::wstring & name) {
            if (is)
            {
                if (!str.empty())
                {
                    str += L", ";
                }

                str += name;
            }
        };

        appendFlagIf(m_options.background, L"background");
        appendFlagIf(m_options.dry_run, L"dry_run");
        appendFlagIf(m_options.skip_file_read, L"skip_file_read");
        appendFlagIf(m_options.verbose, L"verbose");
        appendFlagIf(m_options.show_relative_path, L"show_relative_path");

        if (m_options.ignore_access_error || m_options.ignore_extra || m_options.ignore_unknown ||
            m_options.ignore_warnings)
        {
            appendFlagIf(true, L"ignore_");

            // clang-format off
        if (m_options.ignore_access_error)  { str += ((str.back() == L'_') ? L"" : L"/"); str += L"access";   }
        if (m_options.ignore_extra)         { str += ((str.back() == L'_') ? L"" : L"/"); str += L"extras";   }
        if (m_options.ignore_extra)         { str += ((str.back() == L'_') ? L"" : L"/"); str += L"unknowns"; }
        if (m_options.ignore_extra)         { str += ((str.back() == L'_') ? L"" : L"/"); str += L"warnings"; }
            // clang-format on
        }

        // only show the color option if it is not set to the default value
        if (Options::isColorEnabledByDefault() != m_output.color())
        {
            appendFlagIf(m_output.color(), L"color_on");
            appendFlagIf(!m_output.color(), L"color_off");
        }

        if (m_options.verbose)
        {
            if (!str.empty())
            {
                str += L", ";
            }

            str += L"total_detected_threads=";
            str += std::to_wstring(m_options.thread_counts.total_detected);
            str += L", dir_compare_threads=";
            str += std::to_wstring(m_options.thread_counts.dir_compare);
            str += L", file_compare_threads=";
            str += std::to_wstring(m_options.thread_counts.file_compare);
            str += L", copy_threads=";
            str += std::to_wstring(m_options.thread_counts.copy);
            str += L", delete_threads=";
            str += std::to_wstring(m_options.thread_counts.remove);
        }

        if (!str.empty())
        {
            str = (L"   (" + str + L")");
            printLine(str);
        }
    }

    void BaseOptionsAndOutput::printConflictingOptionsWarnings()
    {
        if ((Job::Cull == m_options.job) && m_options.ignore_extra)
        {
            m_options.ignore_extra = false;

            printLine(
                L"Warning:  The --ignore-extra option disabled by the --cull option.",
                Color::Yellow);
        }

        if (m_options.quiet && m_options.verbose)
        {
            m_options.quiet = false;
            printLine(
                L"Warning:  The --quiet option disabled by the --verbose option.", Color::Yellow);
        }
    }

    void BaseOptionsAndOutput::printLine(std::wstring_view str, const Color color)
    {
        if (m_options.quiet && (Color::Red != color))
        {
            return;
        }

        m_output.print(str, color);
    }

    void BaseOptionsAndOutput::printLineToConsoleOnly(std::wstring_view str, const Color color)
    {
        if (m_options.quiet && (Color::Red != color))
        {
            return;
        }

        m_output.printToConsoleOnly(str, color);
    }

    void BaseOptionsAndOutput::printLineToLogfileOnly(std::wstring_view str)
    {
        m_output.printToLogfileOnly(str);
    }

    void BaseOptionsAndOutput::printEvent(
        const std::wstring & category,
        const std::wstring & name,
        const WhichDir whichDir,
        const bool isFile,
        const std::wstring & path,
        const std::wstring & error,
        const Color color)
    {
        std::wostringstream ss;

        ss.width(12);
        ss << std::left << category;

        ss.width(10);
        ss << std::left << name;

        ss << L" " << toStringShort(whichDir) << L"   ";

        if (isFile)
        {
            ss << "f   ";
        }
        else
        {
            ss << "d   ";
        }

        if (m_options.show_relative_path)
        {
            streamRelativePath(ss, whichDir, path);
        }
        else
        {
            ss << path;
        }

        if (!error.empty())
        {
            ss << L"   {" << error << L"}";
        }

        printLine(ss.str(), color);
    }

    void BaseOptionsAndOutput::printEntryEvent(
        const std::wstring & category,
        const std::wstring & name,
        const Entry & entry,
        const std::wstring & error,
        const Color color)
    {
        // some system calls are returning strings with newlines in them on windows 7
        // didn't bother to spend the time tracking them down
        // this hack simply removes all the non-displayable chars from the error string
        std::wstring errorCleaned{ error };
        //
        errorCleaned.erase(
            std::remove_if(
                std::begin(errorCleaned),
                std::end(errorCleaned),
                [](const wchar_t CH) { return ((CH < 32) || (CH == 127)); }),
            std::end(errorCleaned));

        printEvent(
            category,
            name,
            entry.which_dir,
            entry.is_file,
            entry.path.wstring(),
            errorCleaned,
            color);
    }

    void BaseOptionsAndOutput::printWarningEvent(
        const std::wstring & name,
        const WhichDir whichDir,
        const bool isFile,
        const std::wstring & path,
        const std::wstring & message)
    {
        if (!m_options.ignore_warnings)
        {
            printEvent(L"Warning", name, whichDir, isFile, path, message, Color::Gray);
        }
    }

    void BaseOptionsAndOutput::setOptions(const std::vector<std::string> & args)
    {
        m_output.color(Options::isColorEnabledByDefault());
        setOptions_FromCommandLineArgs(args);
        setOptions_ThreadCounts();
        setOptions_SourceAndDestinationDirectories();
    }

    void BaseOptionsAndOutput::setOptions_ThreadCounts()
    {
        m_options.thread_counts.total_detected =
            static_cast<std::size_t>(std::thread::hardware_concurrency());

        if (m_options.background)
        {
            m_options.thread_counts.dir_compare  = 1;
            m_options.thread_counts.file_compare = 1;
        }
        else
        {
            // this way of deciding how many of the detected threads are used by each type of task
            // was tested and worked well on a variety of platforms, meaning the app did not bring
            // the computer to a grinding halt, but also managed to get all cores burning around
            // 85-95%
            const std::size_t total{ std::clamp(
                m_options.thread_counts.total_detected, 1_st, 64_st) };

            const std::size_t quarterPlusOne{ ((total < 4) ? 1 : (total / 4)) + 1 };
            m_options.thread_counts.dir_compare = quarterPlusOne;

            const std::size_t halfPlusOne{ ((total < 2) ? 1 : (total / 2)) + 1 };
            m_options.thread_counts.file_compare = halfPlusOne;
        }

        if (Job::Copy == m_options.job)
        {
            m_options.thread_counts.copy = m_options.thread_counts.file_compare;
        }

        if (Job::Cull == m_options.job)
        {
            m_options.thread_counts.remove = m_options.thread_counts.file_compare;
        }

        if (m_options.skip_file_read)
        {
            m_options.thread_counts.dir_compare += (m_options.thread_counts.file_compare / 2);
        }
    }

    void BaseOptionsAndOutput::setOptions_SourceAndDestinationDirectories()
    {
        if (m_options.path_str_dpair.src.empty())
        {
            printAndThrow(L"No source directory.");
        }
        else if (m_options.path_str_dpair.dst.empty())
        {
            printAndThrow(L"No destination directory.");
        }
        else
        {
            m_options.entry_dpair.src = Entry(WhichDir::Source, false, options().path_dpair.src, 0);

            m_options.entry_dpair.dst =
                Entry(WhichDir::Destination, false, options().path_dpair.dst, 0);
        }
    }

    void BaseOptionsAndOutput::setOptions_FromCommandLineArgs(const std::vector<std::string> & args)
    {
        if (args.size() <= 1)
        {
            printUsage();
        }

        for (const auto & arg : args)
        {
            // if not an option string, then the arg must be considered one of the two required
            // paths
            if (!setOptions_IfOptionString(arg))
            {
                setOptions_setPath(arg);
            }
        }
    }

    bool BaseOptionsAndOutput::setOptions_IfOptionString(const std::string & arg)
    {
        if (arg == "--compare")
        {
            m_options.job = Job::Compare;
        }
        else if (arg == "--copy")
        {
            m_options.job = Job::Copy;
        }
        else if (arg == "--cull")
        {
            m_options.job = Job::Cull;
        }
        else if (arg == "--dry-run")
        {
            m_options.dry_run = true;
        }
        else if (arg == "--background")
        {
            m_options.background = true;
        }
        else if (arg == "--verbose")
        {
            m_options.verbose = true;
        }
        else if (arg == "--quiet")
        {
            m_options.quiet = true;
        }
        else if ((arg == "--skip-file-read") || (arg == "--skip-file-reads"))
        {
            m_options.skip_file_read = true;
        }
        else if (arg == "--ignore-access")
        {
            m_options.ignore_access_error = true;
        }
        else if ((arg == "--ignore-extra") || (arg == "--ignore-extras"))
        {
            m_options.ignore_extra = true;
        }
        else if ((arg == "--ignore-unknown") || (arg == "--ignore-unknowns"))
        {
            m_options.ignore_unknown = true;
        }
        else if ((arg == "--ignore-warning") || (arg == "--ignore-warnings"))
        {
            m_options.ignore_warnings = true;
        }
        else if (arg == "--ignore-all")
        {
            m_options.ignore_access_error = true;
            m_options.ignore_extra        = true;
            m_options.ignore_unknown      = true;
            m_options.ignore_warnings     = true;
        }
        else if (arg == "--show-relative")
        {
            m_options.show_relative_path = true;
        }
        else if (arg == "--show-absolute")
        {
            m_options.show_relative_path = false;
        }
        else if (
            (arg == "--show-color") || (arg == "--show-colors") || (arg == "--color") ||
            (arg == "--colors") || (arg == "--color-on") || (arg == "--colors-on"))
        {
            m_output.color(true);
        }
        else if (
            (arg == "--hide-color") || (arg == "--hide-colors") || (arg == "--no-color") ||
            (arg == "--no-colors") || (arg == "--color-off") || (arg == "--colors-off"))
        {
            m_output.color(false);
        }
        else if ((arg == "--help") || (arg == "-h") || (arg == "/?"))
        {
            printUsage();
            throw silent_runtime_error();
        }
        else
        {
            return false;
        }

        return true;
    }

    std::wstring BaseOptionsAndOutput::setOptions_MakePathString(const std::string & arg)
    {
        std::string pathStr{ arg };

        // remove wrapping quotes
        strutil::trimIf(
            pathStr, [](const auto ch) { return (strutil::isWhitespace(ch) || (ch == '\"')); });

        // windows drive letters don't work without the trailing slash, so add it here
        if ((pathStr.size() == 2) && strutil::isAlpha(pathStr.at(0)) && (pathStr.at(1) == ':'))
        {
            pathStr += fs::path::preferred_separator;
        }

        return strutil::toWideString(pathStr);
    }

    void BaseOptionsAndOutput::setOptions_setPath(const std::string & arg)
    {
        const std::wstring pathStr{ setOptions_MakePathString(arg) };

        if (m_options.path_str_dpair.src.empty())
        {
            setOptions_setPathhSpecific(WhichDir::Source, pathStr);
        }
        else if (m_options.path_str_dpair.dst.empty())
        {
            setOptions_setPathhSpecific(WhichDir::Destination, pathStr);
        }
        else
        {
            printAndThrow(L"Extra/Incorrect argument: \"" + pathStr + L"\"");
        }
    }

    void BaseOptionsAndOutput::setOptions_setPathhSpecific(
        const WhichDir whichDir, const std::wstring & pathStrOrig)
    {
        printAndThrowIf(whichDir, pathStrOrig.empty(), pathStrOrig, L"Path is empty");

        fs::path & pathObj{ m_options.path_dpair.get(whichDir) };

        pathObj = fs::absolute(fs::path(pathStrOrig));

        printAndThrowIf(
            whichDir,
            (pathObj.empty() || pathObj.wstring().empty()),
            pathObj.wstring(),
            L"Path could not be made absolute");

        ErrorCode_t errorCode;
        const bool doesExist{ fs::exists(pathObj, errorCode) };
        printAndThrowIfErrorCode(whichDir, errorCode, pathObj.wstring(), L"Path does not exist");

        printAndThrowIf(
            whichDir,
            !doesExist,
            pathObj.wstring(),
            L"Path does not exist (after cleanup and making absolute)");

        errorCode.clear();
        const fs::file_status status{ fs::symlink_status(pathObj, errorCode) };
        printAndThrowIfErrorCode(
            whichDir,
            errorCode,
            pathObj.wstring(),
            L"Path failed symlink_status()" + pathObj.wstring());

        printAndThrowIf(
            whichDir,
            !fs::is_directory(status),
            pathObj.wstring(),
            (std::wstring(L"Path is a ") + toString(status.type()) +
             L", which is not a kind of supported directory on this operating system."));

        m_options.path_str_dpair.get(whichDir) = pathObj.wstring();
    }

    void BaseOptionsAndOutput::streamRelativePath(
        std::wostream & os, const WhichDir whichDir, const std::wstring & pathStrOrig) const
    {
        const auto origStrIterEnd{ std::cend(pathStrOrig) };

        const auto & absoluteStr{ m_options.path_str_dpair.get(whichDir) };

        auto origStrIter{ std::mismatch(
                              std::cbegin(pathStrOrig),
                              origStrIterEnd,
                              std::cbegin(absoluteStr),
                              std::cend(absoluteStr))
                              .first };

        // skip past all leading slashes
        while ((origStrIter != origStrIterEnd) && isDirectorySeparator(*origStrIter))
        {
            ++origStrIter;
        }

        while (origStrIter != origStrIterEnd)
        {
            os << *origStrIter++;
        }
    }

    void BaseOptionsAndOutput::printAndThrow(const std::wstring & errorMessage)
    {
        printLine(L"Error: " + errorMessage + L" (consider trying --help)", Color::Red);
        throw silent_runtime_error();
    }

    void BaseOptionsAndOutput::printAndThrowIf(
        const WhichDir whichDir,
        const bool isError,
        const std::wstring & path,
        const std::wstring & error)
    {
        if (isError)
        {
            printAndThrow(error + L"  " + toStringShort(whichDir) + L"  \"" + path + L"\"");
        }
    }

    void BaseOptionsAndOutput::printAndThrowIfErrorCode(
        const WhichDir whichDir,
        const ErrorCode_t & errorCode,
        const std::wstring & pathStr,
        const std::wstring & message)
    {
        return printAndThrowIf(
            whichDir, !!errorCode, pathStr, (message + L" (" + toString(errorCode) + L")"));
    }

} // namespace backup
