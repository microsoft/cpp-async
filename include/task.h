// © Microsoft Corporation. All rights reserved.

#pragma once

#include <coroutine>
#include <exception>
#include <memory>
#include "atomic_acq_rel.h"
#include "awaitable_result.h"

namespace details
{
    template<typename T>
    struct task_state final
    {
        atomic_acq_rel<void*> stateOrCompletion;
        awaitable_result<T> result;

        void* running_state() const noexcept { return const_cast<task_state*>(this); };

        void* ready_state() const noexcept { return const_cast<decltype(result)*>(std::addressof(result)); }

        constexpr void* done_state() const noexcept { return nullptr; };

        bool is_running() const noexcept { return stateOrCompletion == running_state(); }

        bool is_ready() const noexcept { return stateOrCompletion == ready_state(); }

        bool is_done() const noexcept { return stateOrCompletion == done_state(); }

        bool is_completion(void* value)
        {
            return value != running_state() && value != ready_state() && value != done_state();
        }
    };

    template<typename T>
    struct task_promise_type;
}

template<typename T>
struct task final
{
    explicit task(const std::shared_ptr<details::task_state<T>>& state) : m_state{ state } {}

    using promise_type = details::task_promise_type<T>;

    constexpr bool await_ready() const noexcept
    {
        void* currentStateOrCompletion{ m_state->stateOrCompletion };

        return currentStateOrCompletion == m_state->ready_state() || currentStateOrCompletion == m_state->done_state();
    }

    constexpr bool await_suspend(std::coroutine_handle<> handle) const
    {
        void* handleAddress{ handle.address() };

        // In certain situations, the compiler knows there is no code to run in the continuation.
        // Always let treat such empty continuations as "run now" so we preserve nullptr as a sentinel value.
        if (handleAddress == nullptr)
        {
            return false;
        }

        void* currentStateOrCompletion{ m_state->running_state() };

        if (!m_state->stateOrCompletion.compare_exchange_strong(currentStateOrCompletion, handleAddress))
        {
            if (currentStateOrCompletion != m_state->ready_state())
            {
                throw std::runtime_error{ "task<T> may be co_awaited (or have await_suspend() used) only once." };
            }

            return false;
        }

        return true;
    }

    constexpr T await_resume() const
    {
        void* currentStateOrCompletion{ m_state->ready_state() };

        if (!m_state->stateOrCompletion.compare_exchange_strong(currentStateOrCompletion, m_state->done_state()))
        {
            if (currentStateOrCompletion == m_state->done_state())
            {
                throw std::runtime_error{ "task<T> may be co_awaited (or have await_resume() used) only once." };
            }
            else
            {
                throw std::runtime_error{
                    "task<T>.await_resume() may not be called before await_ready() returns true."
                };
            }
        }

        return m_state->result();
    }

private:
    std::shared_ptr<details::task_state<T>> m_state;
};

namespace details
{
    template<typename T>
    struct symmetric_transfer final
    {
        constexpr explicit symmetric_transfer(std::weak_ptr<task_state<T>>&& state) : m_state{ std::move(state) } {}

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept
        {
            std::shared_ptr<task_state<T>> state{ m_state.lock() };

            if (state)
            {
                void* stateOrCompletion{ state->stateOrCompletion.exchange(state->ready_state()) };

                if (state->is_completion(stateOrCompletion))
                {
                    return std::coroutine_handle<task_promise_type<T>>::from_address(stateOrCompletion);
                }
            }

            return std::noop_coroutine();
        }

        constexpr void await_resume() const noexcept {}

    private:
        std::weak_ptr<task_state<T>> m_state;
    };

    template<typename T>
    struct task_promise_type final
    {
        constexpr task<T> get_return_object() noexcept
        {
            std::shared_ptr<task_state<T>> state{ std::make_shared<task_state<T>>() };
            state->stateOrCompletion = state->running_state();
            m_state = std::weak_ptr{ state };
            return task<T>{ std::move(state) };
        }

        constexpr std::suspend_never initial_suspend() const noexcept { return {}; }

        symmetric_transfer<T> final_suspend() const noexcept
        {
            return symmetric_transfer<T>{ std::weak_ptr<task_state<T>>{ std::move(m_state) } };
        }

        constexpr void unhandled_exception() const noexcept
        {
            std::shared_ptr<task_state<T>> state{ m_state.lock() };

            if (state)
            {
                state->result.set_exception(std::current_exception());
            }
        }

        constexpr void return_value(T value) const noexcept
        {
            std::shared_ptr<task_state<T>> state{ m_state.lock() };

            if (state)
            {
                state->result.set_value(std::forward<T>(value));
            }
        }

    private:
        std::weak_ptr<task_state<T>> m_state;
    };

    template<>
    struct task_promise_type<void> final
    {
        task<void> get_return_object() noexcept
        {
            std::shared_ptr<task_state<void>> state{ std::make_shared<task_state<void>>() };
            state->stateOrCompletion = state->running_state();
            m_state = std::weak_ptr{ state };
            return task<void>{ std::move(state) };
        }

        constexpr std::suspend_never initial_suspend() const noexcept { return {}; }

        symmetric_transfer<void> final_suspend() const noexcept
        {
            return symmetric_transfer<void>{ std::weak_ptr<task_state<void>>{ std::move(m_state) } };
        }

        void unhandled_exception() const noexcept
        {
            std::shared_ptr<task_state<void>> state{ m_state.lock() };

            if (state)
            {
                state->result = awaitable_result<void>{ std::current_exception() };
            }
        }

        constexpr void return_void() const noexcept {}

    private:
        std::weak_ptr<task_state<void>> m_state;
    };
}
