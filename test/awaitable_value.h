// © Microsoft Corporation. All rights reserved.

#pragma once

#include <cassert>
#include <coroutine>
#include <memory>

template<typename T>
struct awaitable_value final
{
    explicit awaitable_value(T value) noexcept : m_value{ std::move(value) } {}

    [[nodiscard]] constexpr bool await_ready() const noexcept { return true; }

    [[nodiscard]] constexpr bool await_suspend(std::coroutine_handle<>) const noexcept
    {
        assert(false);
        return false;
    }

    [[nodiscard]] T await_resume() noexcept { return std::move(m_value); }

private:
    T m_value;
    std::unique_ptr<bool> m_checkMoveOnlyTypeAtCompileTime;
};
