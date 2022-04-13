// © Microsoft Corporation. All rights reserved.

#pragma once

#include <atomic>
#include <coroutine>
#include <stdexcept>
#include "atomic_acq_rel.h"
#include "event_signal.h"
#include "simplejthread.h"

struct callback_thread final
{
    callback_thread() : m_wait{}, m_callback{}, m_thread{ [this]() { run(); } } {}

    callback_thread(const callback_thread&) = delete;
    callback_thread(callback_thread&&) noexcept = delete;

    ~callback_thread() { m_wait.set(); }

    callback_thread& operator=(const callback_thread&) = delete;
    callback_thread& operator=(callback_thread&&) noexcept = delete;

    void run() const noexcept
    {
        m_wait.wait_for_or_throw(std::chrono::seconds{ 1 });

        void* callback{ m_callback.load() };

        if (callback != nullptr)
        {
            std::coroutine_handle<>::from_address(callback)();
        }
    }

    bool is_this_thread() const noexcept { return m_thread.get_id() == std::this_thread::get_id(); }

    void callback(std::coroutine_handle<> handle)
    {
        void* expected{ nullptr };
        void* desired{ handle.address() };

        if (!m_callback.compare_exchange_strong(expected, desired))
        {
            throw std::runtime_error{ "A callback may be enqueued only once." };
        }
    }

    void resume() { m_wait.set(); }

    void enqueue(std::coroutine_handle<> handle)
    {
        callback(handle);
        resume();
    }

    event_signal m_wait;
    atomic_acq_rel<void*> m_callback;
    // Switch to std::jthread once we're c++20.
    const simplejthread m_thread;
};
