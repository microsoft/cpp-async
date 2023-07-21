// © Microsoft Corporation. All rights reserved.

#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <memory>
#include "atomic_acq_rel.h"
#include "awaitable_result.h"

namespace async::details
{
    template<typename T>
    struct task_promise_type;

    template<typename T>
    struct task_state final
    {
        atomic_acq_rel<void*> stateOrCompletion;
        awaitable_result<T> result;

        constexpr static std::shared_ptr<task_state> create_shared() noexcept
        {
            std::shared_ptr<task_state> result{ std::make_shared<task_state>() };
            result->stateOrCompletion = result->running_state();
            return result;
        }

        [[nodiscard]] void* running_state() const noexcept { return const_cast<task_state*>(this); };

        [[nodiscard]] void* ready_state() const noexcept
        {
            return const_cast<decltype(result)*>(std::addressof(result));
        }

        [[nodiscard]] constexpr void* done_state() const noexcept { return nullptr; };

        [[nodiscard]] bool is_running() const noexcept { return stateOrCompletion == running_state(); }

        [[nodiscard]] bool is_ready() const noexcept { return stateOrCompletion == ready_state(); }

        [[nodiscard]] bool is_done() const noexcept { return stateOrCompletion == done_state(); }

        [[nodiscard]] bool is_completion(void* value)
        {
            return value != running_state() && value != ready_state() && value != done_state();
        }

        [[nodiscard]] std::coroutine_handle<> mark_ready() noexcept
        {
            // mark_ready() must not be called if the task state has already moved to "done".
            assert(!is_done());

            void* previousStateOrCompletion{ stateOrCompletion.exchange(ready_state()) };

            if (is_completion(previousStateOrCompletion))
            {
                return std::coroutine_handle<task_promise_type<T>>::from_address(previousStateOrCompletion);
            }
            else
            {
                return std::coroutine_handle<>{};
            }
        }
    };

    template<typename T>
    struct task_promise_type;
}

namespace async
{
    template<typename T>
    struct task final
    {
        explicit task(const std::shared_ptr<details::task_state<T>>& state) : m_state{ state } {}

        using promise_type = details::task_promise_type<T>;

        [[nodiscard]] constexpr bool await_ready() const noexcept
        {
            void* currentStateOrCompletion{ m_state->stateOrCompletion };

            return currentStateOrCompletion == m_state->ready_state() ||
                   currentStateOrCompletion == m_state->done_state();
        }

        [[nodiscard]] constexpr bool await_suspend(std::coroutine_handle<> handle) const
        {
            void* handleAddress{ handle.address() };

            // In previous testing, it appeared that a non-empty continuation handle could have a nullptr .address(),
            // perhaps related to compiler or other optimizations, when it is known that no code actually needs to be
            // run when continuing. Attempts to reproduce that behavior have been unsuccessful, but still handle that
            // situation just in case. Always treat such empty continuations as "run now" so we preserve nullptr as a
            // sentinel value.
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

        [[nodiscard]] constexpr T await_resume() const
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
}

namespace async::details
{
    template<typename T>
    constexpr std::coroutine_handle<> get_completion(const std::weak_ptr<task_state<T>>& state)
    {
        std::shared_ptr<task_state<T>> sharedState{ state.lock() };

        if (sharedState)
        {
            std::coroutine_handle<> possibleCompletion{ sharedState->mark_ready() };

            if (possibleCompletion)
            {
                return possibleCompletion;
            }
        }

        return std::coroutine_handle<>{};
    }

    template<typename T>
    constexpr void run_completion_if_exists(const std::weak_ptr<task_state<T>>& state)
    {
        std::coroutine_handle<> possibleCompletion{ get_completion(state) };

        if (possibleCompletion)
        {
            possibleCompletion();
        }
    }

    template<typename T>
    struct task_promise_type final
    {
        constexpr task<T> get_return_object() noexcept
        {
            std::shared_ptr<task_state<T>> state{ task_state<T>::create_shared() };
            m_state = std::weak_ptr{ state };
            return task<T>{ std::move(state) };
        }

        constexpr std::suspend_never initial_suspend() const noexcept { return {}; }

        constexpr std::suspend_never final_suspend() const noexcept
        {
            run_completion_if_exists(m_state);
            return {};
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
            std::shared_ptr<task_state<void>> state{ task_state<void>::create_shared() };
            m_state = std::weak_ptr{ state };
            return task<void>{ std::move(state) };
        }

        constexpr std::suspend_never initial_suspend() const noexcept { return {}; }

        constexpr std::suspend_never final_suspend() const noexcept
        {
            run_completion_if_exists(m_state);
            return {};
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
