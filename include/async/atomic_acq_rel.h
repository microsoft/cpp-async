// © Microsoft Corporation. All rights reserved.

#pragma once

#include <atomic>

namespace async::details
{
    // Like std::atomic, but defaults to std::memory_order_acq_rel (or memory_order_acquire or memory_order_release,
    // as appropriate) rather than memory_order_seq_cst.
    // Does not include some parts of std::atomic not currently used by callers.
    template <typename T>
    struct atomic_acq_rel final
    {
        constexpr atomic_acq_rel() noexcept = default;

        constexpr atomic_acq_rel(T desired) noexcept : m_value{ std::forward<T>(desired) } {}

        atomic_acq_rel(const atomic_acq_rel&) = delete;
        atomic_acq_rel(atomic_acq_rel&&) = delete;

        ~atomic_acq_rel() noexcept = default;

        atomic_acq_rel& operator=(const atomic_acq_rel&) = delete;
        atomic_acq_rel& operator=(const atomic_acq_rel&) volatile = delete;
        atomic_acq_rel& operator=(atomic_acq_rel&&) = delete;

        T operator=(T desired) noexcept
        {
            store(std::forward<T>(desired));
            return std::forward<T>(desired);
        }

        T operator=(T desired) volatile noexcept
        {
            store(std::forward<T>(desired));
            return std::forward<T>(desired);
        }

        operator T() const noexcept { return std::forward<T>(load()); }

        operator T() const volatile noexcept { return std::forward<T>(load()); }

        void store(T desired) noexcept { m_value.store(std::forward<T>(desired), std::memory_order_release); }

        void store(T desired) volatile noexcept { m_value.store(std::forward<T>(desired, std::memory_order_release)); }

        T load() const noexcept { return std::forward<T>(m_value.load(std::memory_order_acquire)); }

        T load() const volatile noexcept { return std::forward<T>(m_value.load(std::memory_order_acquire)); }

        T exchange(T desired) noexcept
        {
            return std::forward<T>(m_value.exchange(std::forward<T>(desired), std::memory_order_acq_rel));
        }

        T exchange(T desired) volatile noexcept
        {
            return std::forward<T>(m_value.exchange(std::forward<T>(desired), std::memory_order_acq_rel));
        }

        bool compare_exchange_weak(T& expected, T desired) noexcept
        {
            return m_value.compare_exchange_weak(
                std::forward<T&>(expected), std::forward<T>(desired), std::memory_order_acq_rel);
        }

        bool compare_exchange_weak(T& expected, T desired) volatile noexcept
        {
            return m_value.compare_exchange_weak(
                std::forward<T&>(expected), std::forward<T>(desired), std::memory_order_acq_rel);
        }

        bool compare_exchange_strong(T& expected, T desired) noexcept
        {
            return m_value.compare_exchange_strong(
                std::forward<T&>(expected), std::forward<T>(desired), std::memory_order_acq_rel);
        }

        bool compare_exchange_strong(T& expected, T desired) volatile noexcept
        {
            return m_value.compare_exchange_strong(
                std::forward<T&>(expected), std::forward<T>(desired), std::memory_order_acq_rel);
        }

    private:
        std::atomic<T> m_value;
    };
}
