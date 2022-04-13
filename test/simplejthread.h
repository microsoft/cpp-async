// © Microsoft Corporation. All rights reserved.

#pragma once

#include <thread>

// pre-c++ 20; only the subset needed for usage in these tests
struct simplejthread
{
    simplejthread() noexcept : m_inner{} {}

    template<typename Function, typename... Args>
    explicit simplejthread(Function&& function, Args&&... args) :
        m_inner{ std::forward<Function&&>(function), std::forward<Args&&>(args)... }
    {}

    simplejthread(const simplejthread&) = delete;

    simplejthread(simplejthread&& other) noexcept : m_inner{ std::move(other.m_inner) } {}

    ~simplejthread() noexcept
    {
        if (m_inner.joinable())
        {
            m_inner.join();
        }
    }

    simplejthread& operator=(const simplejthread&) = delete;

    simplejthread& operator=(simplejthread&& other) noexcept
    {
        if (this != &other)
        {
            if (m_inner.joinable())
            {
                m_inner.join();
            }

            m_inner = std::move(other.m_inner);
        }

        return *this;
    }

    [[nodiscard]] std::thread::id get_id() const noexcept { return m_inner.get_id(); }

    [[nodiscard]] bool joinable() const noexcept { return m_inner.joinable(); }

    void join() { m_inner.join(); }

private:
    std::thread m_inner;
};
