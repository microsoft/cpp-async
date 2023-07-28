// © Microsoft Corporation. All rights reserved.

#pragma once

#include <type_traits>
#include <utility>

namespace async::details
{
    template <typename T, typename = std::void_t<void, void>>
    struct co_await_operator_or_self final
    {
        using type = T;
    };

    template <typename T>
    struct co_await_operator_or_self<T, std::void_t<decltype(std::declval<T>().operator co_await()), void>> final
    {
        using type = decltype(std::declval<T>().operator co_await());
    };

    template <typename T>
    struct co_await_operator_or_self<T, std::void_t<void, decltype(operator co_await(std::declval<T>()))>> final
    {
        using type = decltype(operator co_await(std::declval<T>()));
    };

    template <typename T>
    using co_await_t = typename co_await_operator_or_self<T>::type;

    template <typename T>
    struct awaitable_resume final
    {
        using type = decltype(std::declval<co_await_t<T>>().await_resume());
    };
}

namespace async
{
    template <typename T>
    using awaitable_resume_t = typename details::awaitable_resume<T>::type;
}
