// © Microsoft Corporation. All rights reserved.

#pragma once

struct no_default_constructor_move_only final
{
    constexpr no_default_constructor_move_only() noexcept = delete;

    constexpr explicit no_default_constructor_move_only(int value) noexcept : m_value{ value } {}

    no_default_constructor_move_only(const no_default_constructor_move_only&) = delete;

    no_default_constructor_move_only(no_default_constructor_move_only&& other) noexcept : m_value{ other.m_value }
    {
        other.m_value = {};
    }

    ~no_default_constructor_move_only() noexcept = default;

    no_default_constructor_move_only& operator=(const no_default_constructor_move_only&) = delete;

    no_default_constructor_move_only& operator=(no_default_constructor_move_only&& other) noexcept
    {
        if (this != &other)
        {
            m_value = other.m_value;
            other.m_value = {};
        }

        return *this;
    }

    constexpr int get() const noexcept { return m_value; }

    int m_value{};
};
