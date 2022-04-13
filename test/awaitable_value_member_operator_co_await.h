// © Microsoft Corporation. All rights reserved.

#pragma once

#include <cassert>
#include <coroutine>

template<typename T>
struct awaitable_value_member_operator_co_await final
{
    explicit awaitable_value_member_operator_co_await(T value) : m_value{ std::move(value) } {}

    struct awaitable final
    {
        explicit awaitable(T value) : m_value{ std::move(value) } {}

        constexpr bool await_ready() const noexcept { return true; }

        constexpr bool await_suspend(std::coroutine_handle<>) const noexcept
        {
            assert(false);
            return false;
        }

        T await_resume() noexcept { return std::move(m_value); }

    private:
        T m_value;
    };

    awaitable operator co_await() { return awaitable{ m_value }; }

private:
    T m_value;
};
