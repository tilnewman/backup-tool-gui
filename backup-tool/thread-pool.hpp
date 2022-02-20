#ifndef BACKUP_THREAD_POOL_HPP_INCLUDED
#define BACKUP_THREAD_POOL_HPP_INCLUDED
//
// thread-pool.hpp
//

#include <future>

namespace backup
{

    class ThreadPool
    {
      public:
        ThreadPool()
            : m_futures()
        {}

        void add(std::future<bool> && boolFuture) { m_futures.push_back(std::move(boolFuture)); }

        template <typename StatusUpdateFunction_t>
        void waitUntilAllJoinedAndDestroyed(StatusUpdateFunction_t statusUpdateFunction)
        {
            std::size_t sleepCurrentMs{ 0 };
            const std::size_t sleepMaxMs{ 330 };
            const std::size_t sleepIncrementMs{ 5 };

            while (isAnyRunning())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepCurrentMs));
                sleepCurrentMs = std::clamp((sleepCurrentMs + sleepIncrementMs), 0_st, sleepMaxMs);
                statusUpdateFunction();
            }

            joinAndDestroyAll();
        }

      private:
        bool isAnyRunning()
        {
            for (auto & future : m_futures)
            {
                if (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
                {
                    return true;
                }
            }

            return false;
        }

        void joinAndDestroyAll()
        {
            for (auto & future : m_futures)
            {
                future.get();
            }

            m_futures.clear();
        }

      private:
        std::vector<std::future<bool>> m_futures;
    };

} // namespace backup

#endif // BACKUP_THREAD_POOL_HPP_INCLUDED
