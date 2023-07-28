// © Microsoft Corporation. All rights reserved.

#pragma once

#include <coroutine>
#include <memory>
#include "async/atomic_acq_rel.h"
#include "callback_thread.h"

struct awaitable_void_resume_spy final
{
    explicit awaitable_void_resume_spy(callback_thread& thread, async::details::atomic_acq_rel<bool>& waited) :
        m_thread{ thread }, m_waited{ waited }
    {
    }

    [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) { m_thread.enqueue(handle); }

    void await_resume() noexcept
    {
        if (m_thread.is_this_thread())
        {
            m_waited = true;
        }
    }

private:
    callback_thread& m_thread;
    async::details::atomic_acq_rel<bool>& m_waited;
    std::unique_ptr<bool> m_checkMoveOnlyTypeAtCompileTime;
};
