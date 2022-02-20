#ifndef BACKUP_THREAD_EXCEPTIONS_HPP_INCLUDED
#define BACKUP_THREAD_EXCEPTIONS_HPP_INCLUDED
//
// thread-exceptions.hpp
//
#include "counters.hpp"
#include "options.hpp"
#include "task-queue.hpp"
#include "verified-output.hpp"

#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

// TODO remove
#include <iostream>

namespace backup
{

    class ThreadExceptions
    {
      public:
        ThreadExceptions()
            : m_mutex()
            , m_wereAnyThrown(false)
            , m_exceptionPtrs()
        {}

        bool wereAnyThrown() const { return m_wereAnyThrown; }

        void add(const std::exception_ptr & exPtr)
        {
            std::scoped_lock scopedLock(m_mutex);
            m_exceptionPtrs.push_back(exPtr);
            m_wereAnyThrown = true;

            // TODO REMOVE
            try
            {
                std::rethrow_exception(exPtr);
            }
            catch (const std::exception & ex)
            {
                std::cout << " *** Thread Exception: " << ex.what() << std::endl;
            }
            catch (...)
            {
                std::cout << " *** Thread Exception: (unknown)" << std::endl;
            }
        }

        void reThrowFirst()
        {
            std::scoped_lock scopedLock(m_mutex);
            if (m_exceptionPtrs.empty())
            {
                return;
            }

            const auto exceptionPtrToThrow{ m_exceptionPtrs.front() };
            std::rethrow_exception(exceptionPtrToThrow);
        }

        std::wstring makeSummaryString()
        {
            std::scoped_lock scopedLock(m_mutex);

            if (!m_wereAnyThrown)
            {
                return L"";
            }

            std::wostringstream ss;
            ss << L"Found " << std::to_wstring(m_exceptionPtrs.size())
               << L" exceptions thrown from sub-threads:";

            for (std::size_t i(0); i < m_exceptionPtrs.size(); ++i)
            {
                const auto & exceptionPtr{ m_exceptionPtrs.at(i) };

                try
                {
                    if (exceptionPtr != nullptr)
                    {
                        std::rethrow_exception(exceptionPtr);
                    }
                }
                catch (const std::exception & ex)
                {
                    ss << L"\n\t#" << std::to_wstring(i) << L": std::exception: \"" << ex.what()
                       << L"\"";
                }
                catch (...)
                {
                    ss << L"\n\t#" << std::to_wstring(i) << L": unknown_exception";
                }
            }

            return ss.str();
        }

      private:
        mutable std::mutex m_mutex;
        bool m_wereAnyThrown;
        std::vector<std::exception_ptr> m_exceptionPtrs;
    };

} // namespace backup

#endif // BACKUP_THREAD_EXCEPTIONS_HPP_INCLUDED
