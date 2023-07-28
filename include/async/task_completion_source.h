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

        task<T> task() const noexcept { return ::async::task<T>{ m_taskState }; }

        template <typename... Args>
        void set_value(Args&&... args)
        {
            if (!try_set_value(args...))
            {
                throw std::runtime_error{ "The task_completion_source<T> has already been completed." };
            }
        }

        template <typename... Args>
        [[nodiscard]] bool try_set_value(Args&&... args)
        {
            task_completion_state expected{ task_completion_state::unset };

            if (!m_completionState.compare_exchange_strong(expected, task_completion_state::setting))
            {
                return false;
            }

            try
            {
                m_taskState->result.set_value(std::forward<T>(args)...);
            }
            catch (...)
            {
                m_completionState = task_completion_state::unset;
                throw;
            }

            m_completionState = task_completion_state::set;
            complete();

            return true;
        }

        void set_exception(std::exception_ptr exception)
        {
            if (!exception)
            {
                throw std::invalid_argument{ "The exception_ptr must not be empty." };
            }

            if (!try_set_exception(exception))
            {
                throw std::runtime_error{ "The task_completion_source<T> has already been completed." };
            }
        }

        [[nodiscard]] bool try_set_exception(std::exception_ptr exception) noexcept
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

            try
            {
                m_taskState->result.set_exception(exception);
            }
            catch (...)
            {
                m_completionState = task_completion_state::unset;
                std::terminate();
            }

            m_completionState = task_completion_state::set;
            complete();

            return true;
        }

    private:
        void complete() noexcept
        {
            assert(m_completionState.load() == task_completion_state::set);

            std::coroutine_handle<> possibleCompletion{ m_taskState->mark_ready() };

            if (possibleCompletion)
            {
                possibleCompletion();
            }
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
        task<T> task() const noexcept { return m_core.task(); }

        void set_value(T value) { m_core.set_value(value); }

        [[nodiscard]] bool try_set_value(T value) noexcept { return m_core.try_set_value(value); };

        void set_exception(std::exception_ptr exception) { m_core.set_exception(exception); }

        [[nodiscard]] bool try_set_exception(std::exception_ptr exception) noexcept
        {
            return m_core.try_set_exception(exception);
        }

    private:
        details::task_completion_source_core<T> m_core;
    };

    template <>
    struct task_completion_source<void> final
    {
        task<void> task() const noexcept { return m_core.task(); }

        void set_value() { m_core.set_value(); }

        [[nodiscard]] bool try_set_value() noexcept { return m_core.try_set_value(); };

        void set_exception(std::exception_ptr exception) { m_core.set_exception(exception); }

        [[nodiscard]] bool try_set_exception(std::exception_ptr exception) noexcept
        {
            return m_core.try_set_exception(exception);
        }

    private:
        details::task_completion_source_core<void> m_core;
    };
}
