#ifndef BACKUP_BASE_COUNTERS_AND_ERRORS_HPP_INCLUDED
#define BACKUP_BASE_COUNTERS_AND_ERRORS_HPP_INCLUDED
//
// base-counters-and-errors.hpp
//
#include "base-options-and-output.hpp"
#include "counters.hpp"

namespace backup
{

    struct CounterResults
    {
        bool errors;
        bool mismatches;
        bool copies;
        bool removes;
    };

    class BaseCountersAndErrors : public BaseOptionsAndOutput
    {
      protected:
        BaseCountersAndErrors(const std::vector<std::string> & args);
        ~BaseCountersAndErrors() = default;

        void count(const Entry & entry);

        void printAndCountError(
            const Error error, const Entry & entry, const std::wstring & message);

        bool printAndCountStreamErrorIf(
            const InputFileStream_t & fileStream, const Error error, const Entry & entry);

        bool printAndCountErrorCodeIf(
            const ErrorCode_t & errorCode,
            const Error errorEnum,
            const Entry & entry,
            const std::wstring & message = L"");

        void printAndCountMismatch(
            const Mismatch mismatch, const Entry & entry, const std::wstring & message);

        void countCopy(const Entry & entry);
        void countRemove(const Entry & entry);

        CounterResults printCounterResults();

      private:
        TreeCounter m_copyCounter;
        TreeCounter m_removeCounter;
        TreeCounter m_mismatchCounter;
        TreeCounter m_srcTreeCounter;
        TreeCounter m_dstTreeCounter;
    };

} // namespace backup

#endif // BACKUP_BASE_COUNTERS_AND_ERRORS_HPP_INCLUDED
