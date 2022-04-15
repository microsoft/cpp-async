// © Microsoft Corporation. All rights reserved.

#pragma once

#include <cassert>
#include <coroutine>
#include <exception>

struct awaitable_void_throws final
{
    explicit awaitable_void_throws(std::exception_ptr exception) noexcept : m_exception{ exception } {}

    [[nodiscard]] constexpr bool await_ready() const noexcept { return true; }

    [[nodiscard]] constexpr bool await_suspend(std::coroutine_handle<>) const noexcept
    {
        assert(false);
        return false;
    }

    void await_resume() const { std::rethrow_exception(m_exception); }

private:
    std::exception_ptr m_exception;
};
