// © Microsoft Corporation. All rights reserved.

#pragma once

#include <coroutine>
#include <memory>
#include "async/atomic_acq_rel.h"
#include "callback_thread.h"

template<typename T>
struct awaitable_value_resume_spy final
{
    awaitable_value_resume_spy(callback_thread& thread, async::details::atomic_acq_rel<bool>& waited, T value) :
        m_thread{ thread }, m_waited{ waited }, m_value{ std::forward<T>(value) }
    {}

    [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) { m_thread.enqueue(handle); }

    [[nodiscard]] T await_resume() noexcept
    {
        if (m_thread.is_this_thread())
        {
            m_waited = true;
        }

        return std::forward<T>(m_value);
    }

private:
    callback_thread& m_thread;
    async::details::atomic_acq_rel<bool>& m_waited;
    T m_value;
    std::unique_ptr<bool> m_checkMoveOnlyTypeAtCompileTime;
};
