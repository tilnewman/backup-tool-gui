#ifndef BACKUP_BASE_OPTIONS_AND_OUTPUT_HPP_INCLUDED
#define BACKUP_BASE_OPTIONS_AND_OUTPUT_HPP_INCLUDED
//
// base-options-and-output.hpp
//
#include "counters.hpp"
#include "entry.hpp"
#include "options.hpp"
#include "task-queue.hpp"
#include "thread-exceptions.hpp"
#include "verified-output.hpp"

#include <string>
#include <vector>

namespace backup
{

    // This base class is responsible for converting command line arguments into options, and for
    // wrapping all print/output operations.
    class BaseOptionsAndOutput
    {
      protected:
        BaseOptionsAndOutput(const std::vector<std::string> & args);
        ~BaseOptionsAndOutput() = default;

        inline const Options & options() const noexcept { return m_options; }

        void printLine(std::wstring_view str, const Color color = Color::Default);
        void printLineToConsoleOnly(std::wstring_view str, const Color color = Color::Default);
        void printLineToLogfileOnly(std::wstring_view str);

        void printEvent(
            const std::wstring & category,
            const std::wstring & name,
            const WhichDir whichDir,
            const bool isFile,
            const std::wstring & path,
            const std::wstring & error = L"",
            const Color color          = Color::Default);

        void printEntryEvent(
            const std::wstring & category,
            const std::wstring & name,
            const Entry & entry,
            const std::wstring & error = L"",
            const Color color          = Color::Default);

        void printWarningEvent(
            const std::wstring & name,
            const WhichDir whichDir,
            const bool isFile,
            const std::wstring & path,
            const std::wstring & message);

        inline Clock_t::time_point lastPrintTime() const { return m_output.lastPrintTime(); }

        void disableQuietOptionToPrintFinalResults() { m_options.quiet = false; }

      private:
        void setOptions(const std::vector<std::string> & args);
        void setOptions_ThreadCounts();
        void setOptions_SourceAndDestinationDirectories();
        void setOptions_FromCommandLineArgs(const std::vector<std::string> & args);
        bool setOptions_IfOptionString(const std::string & arg);
        std::wstring setOptions_MakePathString(const std::string & arg);
        void setOptions_setPath(const std::string & arg);
        void setOptions_setPathhSpecific(const WhichDir whichDir, const std::wstring & pathStr);

        void printUsage();
        void printJobSummary(const std::vector<std::string> & args);
        void printOptionsSummary();
        void printConflictingOptionsWarnings();

        void streamRelativePath(
            std::wostream & os, const WhichDir whichDir, const std::wstring & pathStrOrig) const;

        [[noreturn]] void printAndThrow(const std::wstring & errorMessage);

        void printAndThrowIf(
            const WhichDir whichDir,
            const bool isError,
            const std::wstring & path,
            const std::wstring & error);

        void printAndThrowIfErrorCode(
            const WhichDir whichDir,
            const ErrorCode_t & errorCode,
            const std::wstring & path,
            const std::wstring & error);

      private:
        Options m_options;
        VerifiedOutput m_output;
    };

} // namespace backup

#endif // BACKUP_BASE_OPTIONS_AND_OUTPUT_HPP_INCLUDED
