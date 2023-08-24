// © Microsoft Corporation. All rights reserved.

#pragma once

#include <cassert>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <tuple>

namespace async
{
    template <typename T>
    struct awaitable_result final
    {
        constexpr awaitable_result() noexcept : m_storageType{ result_union_type::unset }, m_storage{} {}

        awaitable_result(const awaitable_result&) = delete;

        awaitable_result(awaitable_result&& other) noexcept : m_storageType{ result_union_type::unset }, m_storage{}
        {
            switch (other.m_storageType)
            {
            case result_union_type::exception:
                new (std::addressof(m_storage.exception)) std::exception_ptr{ std::move(other.m_storage.exception) };
                other.m_storage.exception = {};
                break;
            case result_union_type::value:
                new (std::addressof(m_storage.value)) possible_reference{ std::move(other.m_storage.value) };
                break;
            default:
                break;
            }

            m_storageType = other.m_storageType;
            other.m_storageType = result_union_type::unset;
        }

        ~awaitable_result() noexcept
        {
            switch (m_storageType)
            {
            case result_union_type::exception:
                m_storage.exception.~exception_ptr();
                break;
            case result_union_type::value:
                m_storage.value.~possible_reference();
                break;
            }
        }

        awaitable_result& operator=(const awaitable_result&) = delete;
        awaitable_result& operator=(awaitable_result&&) noexcept = delete;

        T operator()()
        {
            switch (m_storageType)
            {
            case result_union_type::exception:
                std::rethrow_exception(m_storage.exception);
                break;
            case result_union_type::value:
                return std::forward<T>(m_storage.value.value);
            case result_union_type::unset:
                assert(false);
                throw std::runtime_error{ "Awaitable result is not yet available." };
            default:
                assert(false);
                throw std::runtime_error{ "Invalid awaitable result state." };
            }
        }

        void set_value(T value) noexcept
        {
            new (std::addressof(m_storage.value)) possible_reference{ std::forward<T>(value) };
            m_storageType = result_union_type::value;
        }

        void set_exception(const std::exception_ptr& exception) noexcept
        {
            new (std::addressof(m_storage.exception)) std::exception_ptr{ exception };
            m_storageType = result_union_type::exception;
        }

    private:
        // A union (result_union below) may not contain a reference type.
        // Wrap the possible reference type in a struct, which may contain a reference type.
        struct possible_reference
        {
            T value;
        };

        union result_union
        {
            constexpr result_union() noexcept { std::ignore = std::memset(this, 0, sizeof(*this)); }

            result_union(const result_union&) = delete;
            result_union(result_union&&) noexcept = delete;

            ~result_union() noexcept {}

            result_union& operator=(const result_union&) = delete;
            result_union& operator=(result_union&&) noexcept = delete;

            possible_reference value;
            std::exception_ptr exception;
        };

        enum class result_union_type
        {
            unset,
            value,
            exception
        };

        result_union_type m_storageType;
        result_union m_storage;
    };

    template <>
    struct awaitable_result<void> final
    {
        awaitable_result() noexcept : m_exception{} {}

        explicit awaitable_result(const std::exception_ptr& exception) noexcept : m_exception{ exception } {}

        awaitable_result(const awaitable_result&) = delete;
        awaitable_result(awaitable_result&&) noexcept = default;

        ~awaitable_result() noexcept = default;

        awaitable_result& operator=(const awaitable_result&) = delete;

        awaitable_result& operator=(awaitable_result&& other) noexcept = default;

        void operator()() const
        {
            if (m_exception)
            {
                std::rethrow_exception(m_exception);
            }
        }

        constexpr void set_value() const noexcept {};

        void set_exception(const std::exception_ptr& exception) noexcept { m_exception = exception; }

    private:
        std::exception_ptr m_exception{};
    };
}
