// © Microsoft Corporation. All rights reserved.

#pragma once

#include <cassert>
#include <coroutine>

struct awaitable_void final
{
    [[nodiscard]] constexpr bool await_ready() const noexcept { return true; }

    [[nodiscard]] constexpr bool await_suspend(std::coroutine_handle<>) const noexcept
    {
        assert(false);
        return false;
    }

    [[nodiscard]] constexpr void await_resume() const noexcept {}
};
