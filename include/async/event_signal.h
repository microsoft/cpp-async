// © Microsoft Corporation. All rights reserved.

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include "atomic_acq_rel.h"

namespace async
{
    struct event_signal final
    {
        event_signal() noexcept : m_signaled{ false } {}

        bool is_set() const
        {
            std::lock_guard<std::mutex> mutexGuard{ m_mutex };
            return m_signaled.load();
        }

        void wait() const
        {
            std::unique_lock<std::mutex> mutexLock{ m_mutex };
            m_condition.wait(mutexLock, [this]() noexcept { return m_signaled.load(); });
        }

        template <typename Rep, typename Period>
        bool wait_for(const std::chrono::duration<Rep, Period>& rel_time) const
        {
            std::unique_lock<std::mutex> mutexLock{ m_mutex };
            return m_condition.wait_for(mutexLock, rel_time, [this]() { return m_signaled.load(); });
        }

        template <typename Rep, typename Period>
        void wait_for_or_throw(const std::chrono::duration<Rep, Period>& rel_time) const
        {
            if (!wait_for(rel_time))
            {
                throw std::runtime_error{ "Wait timed out." };
            }
        }

        void set() noexcept
        {
            // std::lock_guard constructor is not technically noexcept, but the only possible exception would be from
            // std::mutex.lock, which itself is not technically noexcept, but the underlying implementation on MSVC
            // (_Mtx_lock) appears never to return failure/throw (ultimately calls AcquireSRWLockExclusive, which never
            // fails).
            // Regardless, the C++20 standard requires that co_await of a final_suspend not be potentially throwing, and
            // this member is designed to be called by a final_suspend for awaitable_get, so treat any exception from
            // other Standard Library implementations here as fatal.
            std::lock_guard<std::mutex> mutexGuard{ m_mutex };
            m_signaled = true;
            m_condition.notify_all();
        }

    private:
        mutable std::condition_variable m_condition;
        mutable std::mutex m_mutex;
        details::atomic_acq_rel<bool> m_signaled;
    };
}
