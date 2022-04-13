// © Microsoft Corporation. All rights reserved.

#pragma once

#include <cassert>
#include <coroutine>

struct awaitable_void final
{
    constexpr bool await_ready() const noexcept { return true; }

    constexpr bool await_suspend(std::coroutine_handle<>) const noexcept
    {
        assert(false);
        return false;
    }

    constexpr void await_resume() const noexcept {}
};
