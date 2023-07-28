// © Microsoft Corporation. All rights reserved.

#pragma once

#include <coroutine>
#include <future>
#include "awaitable_resume_t.h"

namespace async::details
{
    struct to_future_task final
    {
        struct promise_type final
        {
            constexpr to_future_task get_return_object() const noexcept { return {}; }
            constexpr std::suspend_never initial_suspend() const noexcept { return {}; }
            void unhandled_exception() const { std::rethrow_exception(std::current_exception()); }
            constexpr void return_void() const noexcept {}
            constexpr std::suspend_never final_suspend() const noexcept { return {}; }
        };
    };

    template <typename T, typename Awaitable>
    struct to_future_factory final
    {
        static to_future_task create(Awaitable awaitable, std::future<T>& future)
        {
            std::promise<T> promise{};
            future = promise.get_future();

            try
            {
                promise.set_value(co_await std::move(awaitable));
            }
            catch (...)
            {
                promise.set_exception(std::current_exception());
            }

            co_return;
        }
    };

    template <typename Awaitable>
    struct to_future_factory<void, Awaitable> final
    {
        static to_future_task create(Awaitable awaitable, std::future<void>& future)
        {
            std::promise<void> promise{};
            future = promise.get_future();

            std::exception_ptr exception{};

            try
            {
                co_await awaitable;
            }
            catch (...)
            {
                exception = std::current_exception();
            }

            if (exception)
            {
                promise.set_exception(exception);
            }
            else
            {
                promise.set_value();
            }

            co_return;
        }
    };
}

namespace async
{
    template <typename Awaitable>
    std::future<awaitable_resume_t<Awaitable>> to_future(Awaitable awaitable)
    {
        using T = awaitable_resume_t<Awaitable>;
        std::future<T> result{};
        details::to_future_factory<T, Awaitable>::create(std::move(awaitable), result);
        return result;
    }
}
