// © Microsoft Corporation. All rights reserved.

#pragma once

#include <memory>
#include "atomic_acq_rel.h"
#include "task.h"

namespace async::details
{
    enum class task_completion_state
    {
        unset,
        setting,
        set
    };

    template <typename T>
    struct task_completion_source_core final
    {
        task_completion_source_core() :
            m_taskState{ task_state<T>::create_shared() }, m_completionState{ task_completion_state::unset }
        {
        }

        ::async::task<T> task() const noexcept { return ::async::task<T>{ m_taskState }; }

        template <typename... Args>
        void set_value(Args&&... args)
        {
            std::exception_ptr completionException{};

            if (!try_set_value(completionException, args...))
            {
                if (completionException)
                {
                    std::rethrow_exception(completionException);
                }

                throw std::runtime_error{ "The task_completion_source<T> has already been completed." };
            }
        }

        template <typename... Args>
        [[nodiscard]] bool try_set_value(std::exception_ptr& completionException, Args&&... args) noexcept
        {
            task_completion_state expected{ task_completion_state::unset };

            if (!m_completionState.compare_exchange_strong(expected, task_completion_state::setting))
            {
                return false;
            }

            m_taskState->result.set_value(std::forward<T>(args)...);
            m_completionState = task_completion_state::set;
            return try_complete(completionException);
        }

        template <typename... Args>
        [[nodiscard]] bool try_set_value_terminate_on_completion_exception(Args&&... args) noexcept
        {
            std::exception_ptr completionException{};

            if (!try_set_value(completionException, args...))
            {
                if (completionException)
                {
                    std::terminate();
                }

                return false;
            }

            return true;
        }

        void set_exception(const std::exception_ptr& exception)
        {
            if (!exception)
            {
                throw std::invalid_argument{ "The exception_ptr must not be empty." };
            }

            std::exception_ptr completionException{};

            if (!try_set_exception(exception, completionException))
            {
                if (completionException)
                {
                    std::rethrow_exception(completionException);
                }

                throw std::runtime_error{ "The task_completion_source<T> has already been completed." };
            }
        }

        [[nodiscard]] bool try_set_exception(
            const std::exception_ptr& exception, std::exception_ptr& completionException) noexcept
        {
            if (!exception)
            {
                return false;
            }

            task_completion_state expected{ task_completion_state::unset };

            if (!m_completionState.compare_exchange_strong(expected, task_completion_state::setting))
            {
                return false;
            }

            m_taskState->result.set_exception(exception);
            m_completionState = task_completion_state::set;
            return try_complete(completionException);
        }

        [[nodiscard]] bool try_set_exception_terminate_on_completion_exception(
            const std::exception_ptr& exception) noexcept
        {
            std::exception_ptr completionException{};

            if (!try_set_exception(exception, completionException))
            {
                if (completionException)
                {
                    std::terminate();
                }

                return false;
            }

            return true;
        }

    private:
        bool try_complete(std::exception_ptr& exception) noexcept
        {
            assert(m_completionState.load() == task_completion_state::set);
            exception = {};

            const std::coroutine_handle<> possibleCompletion{ m_taskState->mark_ready() };

            if (possibleCompletion)
            {
                try
                {
                    possibleCompletion();
                }
                catch (...)
                {
                    exception = std::current_exception();
                    return false;
                }
            }

            return true;
        }

        std::shared_ptr<task_state<T>> m_taskState;
        atomic_acq_rel<task_completion_state> m_completionState;
    };
}

namespace async
{
    template <typename T>
    struct task_completion_source final
    {
        ::async::task<T> task() const noexcept { return m_core.task(); }

        void set_value(T value) { m_core.set_value(value); }

        ///
        /// @deprecated Use try_set_value(T, std::exception_ptr&) instead.
        ///
        [[nodiscard]] bool try_set_value(T value) noexcept
        {
            return m_core.try_set_value_terminate_on_completion_exception(std::forward<T>(value));
        }

        [[nodiscard]] bool try_set_value(T value, std::exception_ptr& completionException) noexcept
        {
            return m_core.try_set_value(completionException, std::forward<T>(value));
        }

        void set_exception(const std::exception_ptr& exception) { m_core.set_exception(exception); }

        ///
        /// @deprecated Use try_set_exception(const std::exception_ptr&, std::exception_ptr&) instead.
        ///
        [[nodiscard]] bool try_set_exception(const std::exception_ptr& exception) noexcept
        {
            return m_core.try_set_exception_terminate_on_completion_exception(exception);
        }

        [[nodiscard]] bool try_set_exception(
            const std::exception_ptr& exception, std::exception_ptr& completionException) noexcept
        {
            return m_core.try_set_exception(exception, completionException);
        }

    private:
        details::task_completion_source_core<T> m_core;
    };

    template <>
    struct task_completion_source<void> final
    {
        ::async::task<void> task() const noexcept { return m_core.task(); }

        void set_value() { m_core.set_value(); }

        ///
        /// @deprecated Use try_set_value(std::exception_ptr&) instead.
        ///
        [[nodiscard]] bool try_set_value() noexcept { return m_core.try_set_value_terminate_on_completion_exception(); }

        [[nodiscard]] bool try_set_value(std::exception_ptr& completionException) noexcept
        {
            return m_core.try_set_value(completionException);
        }

        void set_exception(const std::exception_ptr& exception) { m_core.set_exception(exception); }

        ///
        /// @deprecated Use try_set_exception(const std::exception_ptr&, std::exception_ptr&) instead.
        ///
        [[nodiscard]] bool try_set_exception(const std::exception_ptr& exception) noexcept
        {
            return m_core.try_set_exception_terminate_on_completion_exception(exception);
        }

        [[nodiscard]] bool try_set_exception(
            const std::exception_ptr& exception, std::exception_ptr& completionException) noexcept
        {
            return m_core.try_set_exception(exception, completionException);
        }

    private:
        details::task_completion_source_core<void> m_core;
    };
}
