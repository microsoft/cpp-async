// © Microsoft Corporation. All rights reserved.

#pragma once

#include <coroutine>
#include <exception>
#include "awaitable_result.h"
#include "awaitable_resume_t.h"

namespace async::details
{
    struct then_task final
    {
        struct promise_type final
        {
            constexpr then_task get_return_object() const noexcept { return {}; }
            constexpr std::suspend_never initial_suspend() const noexcept { return {}; }
            void unhandled_exception() const { std::rethrow_exception(std::current_exception()); }
            constexpr void return_void() const noexcept {}
            constexpr std::suspend_never final_suspend() const noexcept { return {}; }
        };
    };

    template <typename T, typename Awaitable, typename Continuation>
    struct then_task_factory final
    {
        static then_task create(Awaitable awaitable, Continuation continuation)
        {
            awaitable_result<T> result{};

            try
            {
                result.set_value(co_await std::move(awaitable));
            }
            catch (...)
            {
                result.set_exception(std::current_exception());
            }

            continuation(std::move(result));
            co_return;
        }
    };

    template <typename Awaitable, typename Continuation>
    struct then_task_factory<void, Awaitable, Continuation> final
    {
        static then_task create(Awaitable awaitable, Continuation continuation)
        {
            std::exception_ptr exception{};

            try
            {
                co_await std::move(awaitable);
            }
            catch (...)
            {
                exception = std::current_exception();
            }

            continuation(awaitable_result<void>{ exception });
            co_return;
        }
    };
}

namespace async
{
    template <typename Awaitable, typename Continuation>
    inline void awaitable_then(Awaitable awaitable, Continuation continuation)
    {
        using T = awaitable_resume_t<Awaitable>;

        details::then_task_factory<T, Awaitable, Continuation>::create(std::move(awaitable), continuation);
    }
}
