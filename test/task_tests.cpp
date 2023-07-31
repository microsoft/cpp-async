// © Microsoft Corporation. All rights reserved.

#include <exception>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <catch2/catch.hpp>
#include "async/task.h"
#include "async/awaitable_then.h"
#include "async/event_signal.h"
#include "awaitable_value_throws.h"
#include "awaitable_void_throws.h"
#include "callback_thread.h"
#include "no_default_constructor_move_only.h"

namespace
{
    async::task<void> task_void_co_return() { co_return; }
}

TEST_CASE("task<void>.await_ready() returns true when task does not suspend")
{
    // Arrange
    async::task<void> task{ task_void_co_return() };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(ready);
}

namespace
{
    struct never_ready_awaitable_void final
    {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}

        void await_resume() const
        {
            assert(false);
            throw std::runtime_error{ "This awaitable never resumes." };
        }
    };

    template <typename Awaitable>
    async::task<void> task_void_co_await(Awaitable awaitable)
    {
        co_await awaitable;
    }
}

TEST_CASE("task<void>.await_ready() returns false when task suspends")
{
    // Arrange
    async::task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(!ready);
}

TEST_CASE("task<void>.await_suspend() returns true when task is suspended")
{
    // Arrange
    async::task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(suspended);
}

TEST_CASE("task<void>.await_suspend() returns false when task is not suspended")
{
    // Arrange
    async::task<void> task{ task_void_co_return() };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(!suspended);
}

TEST_CASE("task<void>.await_suspend() does not run continuation when task is suspended")
{
    // Arrange
    async::task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };
    async::details::atomic_acq_rel<bool> run{ false };
    auto continuation = [&run](async::awaitable_result<void>) { run = true; };

    // Act
    async::awaitable_then(task, continuation);

    // Assert
    REQUIRE(!run);
}

namespace
{
    struct suspend_to_paused_callback_thread_awaitable_void final
    {
        explicit suspend_to_paused_callback_thread_awaitable_void(callback_thread& thread) noexcept : m_thread{ thread }
        {
        }

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) const noexcept { m_thread.callback(handle); }

        constexpr void await_resume() const noexcept {}

    private:
        callback_thread& m_thread;
    };
}

TEST_CASE("task<void>.await_suspend() runs continuation when task completes")
{
    // Arrange
    callback_thread callbackThread{};
    async::task<void> task{ task_void_co_await(suspend_to_paused_callback_thread_awaitable_void{ callbackThread }) };
    async::event_signal done{};
    auto continuation = [&done](async::awaitable_result<void>) { done.set(); };
    async::awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

namespace
{
    struct scope_spy final
    {
        constexpr explicit scope_spy(bool& destroyed) noexcept : m_destroyed{ destroyed } {}

        ~scope_spy() noexcept { m_destroyed = true; }

    private:
        bool& m_destroyed;
    };

    template <typename Awaitable>
    async::task<void> task_void_co_return_co_await_with_scope(bool& scopeDestroyed, Awaitable awaitable)
    {
        scope_spy destroyBeforeContinue{ scopeDestroyed };
        co_await awaitable;
        co_return;
    }
}

TEST_CASE("task<void>.await_suspend() does not run continuation before leaving coroutine scope")
{
    // Arrange
    bool scopeDestroyed{ false };
    bool scopeDestroyedDuringCompletion{};
    callback_thread callbackThread{};
    async::task<void> task{ task_void_co_return_co_await_with_scope(
        scopeDestroyed, suspend_to_paused_callback_thread_awaitable_void{ callbackThread }) };
    async::event_signal done{};
    auto continuation = [&scopeDestroyed, &scopeDestroyedDuringCompletion, &done](async::awaitable_result<void>)
    {
        scopeDestroyedDuringCompletion = scopeDestroyed;
        done.set();
    };
    async::awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    done.wait_for_or_throw(std::chrono::seconds{ 1 });
    REQUIRE(scopeDestroyedDuringCompletion);
}

TEST_CASE("task<void>.await_suspend() throws if another continuation is present")
{
    // Arrange
    async::task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };
    std::noop_coroutine_handle firstHandle{ std::noop_coroutine() };

    if (task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    if (!task.await_suspend(firstHandle))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_suspend(handle), std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_suspend() used) only once."));
}

TEST_CASE("task<void>.await_resume() does not throw when task does not throw")
{
    // Arrange
    async::task<void> task{ task_void_co_return() };

    // Act & Assert
    REQUIRE_NOTHROW(task.await_resume());
}

TEST_CASE("task<void>.await_resume() throws when task throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    async::task<void> task{ task_void_co_await(awaitable_void_throws{ std::make_exception_ptr(expected) }) };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task<void>.await_resume() throws when called before completion")
{
    // Arrange
    async::task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };

    if (task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error,
        Catch::Matchers::Message("task<T>.await_resume() may not be called before await_ready() returns true."));
}

TEST_CASE("task<void>.await_resume() throws when called a second time")
{
    // Arrange
    async::task<void> task{ task_void_co_return() };

    if (!task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    task.await_resume();

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_resume() used) only once."));
}

namespace
{
    template <typename T>
    async::task<T> task_value_co_return(T value)
    {
        co_return value;
    }
}

TEST_CASE("task<T>.await_ready() returns true when task does not suspend")
{
    // Arrange
    constexpr int unusedValue{ 123 };
    async::task<int> task{ task_value_co_return(unusedValue) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(ready);
}

namespace
{
    template <typename T>
    struct never_ready_awaitable_value final
    {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}

        [[nodiscard]] T await_resume() const
        {
            assert(false);
            throw std::runtime_error{ "This awaitable never resumes." };
        }
    };

    template <typename Awaitable>
    auto task_value_co_return_co_await(Awaitable awaitable) -> async::task<decltype(awaitable.await_resume())>
    {
        co_return co_await awaitable;
    }
}

TEST_CASE("task<T>.await_ready() returns false when task suspends")
{
    // Arrange
    async::task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(!ready);
}

TEST_CASE("task<T>.await_suspend() returns true when task is suspended")
{
    // Arrange
    async::task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(suspended);
}

TEST_CASE("task<T>.await_suspend() returns false when task is not suspended")
{
    // Arrange
    constexpr int unusedValue{ 123 };
    async::task<int> task{ task_value_co_return(unusedValue) };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(!suspended);
}

TEST_CASE("task<T>.await_suspend() does not run continuation when task is suspended")
{
    // Arrange
    async::task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };
    async::details::atomic_acq_rel<bool> run{ false };
    auto continuation = [&run](async::awaitable_result<int>) { run = true; };

    // Act
    async::awaitable_then(task, continuation);

    // Assert
    REQUIRE(!run);
}

namespace
{
    template <typename T>
    struct suspend_to_paused_callback_thread_awaitable_value final
    {
        explicit suspend_to_paused_callback_thread_awaitable_value(callback_thread& thread, T value) noexcept :
            m_thread{ thread }, m_value{ value }
        {
        }

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) const noexcept { m_thread.callback(handle); }

        [[nodiscard]] constexpr T await_resume() const noexcept { return m_value; }

    private:
        callback_thread& m_thread;
        T m_value;
    };
}

TEST_CASE("task<T>.await_suspend() runs continuation when task completes")
{
    // Arrange
    callback_thread callbackThread{};
    constexpr int unusedValue{ 123 };
    async::task<int> task{ task_value_co_return_co_await(
        suspend_to_paused_callback_thread_awaitable_value{ callbackThread, unusedValue }) };
    async::event_signal done{};
    auto continuation = [&done](async::awaitable_result<int>) { done.set(); };
    async::awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

namespace
{
    template <typename Awaitable>
    auto task_value_co_return_co_await_with_scope(bool& scopeDestroyed, Awaitable awaitable)
        -> async::task<decltype(awaitable.await_resume())>
    {
        scope_spy destroyBeforeContinue{ scopeDestroyed };
        co_return co_await awaitable;
    }
}

TEST_CASE("task<T>.await_suspend() does not run continuation before leaving coroutine scope")
{
    // Arrange
    bool scopeDestroyed{ false };
    bool scopeDestroyedDuringCompletion{};
    callback_thread callbackThread{};
    constexpr int unusedValue{ 123 };
    async::task<int> task{ task_value_co_return_co_await_with_scope(
        scopeDestroyed, suspend_to_paused_callback_thread_awaitable_value{ callbackThread, unusedValue }) };
    async::event_signal done{};
    auto continuation = [&scopeDestroyed, &scopeDestroyedDuringCompletion, &done](async::awaitable_result<int>)
    {
        scopeDestroyedDuringCompletion = scopeDestroyed;
        done.set();
    };
    async::awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    done.wait_for_or_throw(std::chrono::seconds{ 1 });
    REQUIRE(scopeDestroyedDuringCompletion);
}

TEST_CASE("task<T>.await_suspend() throws if another continuation is present")
{
    // Arrange
    async::task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };
    std::noop_coroutine_handle firstHandle{ std::noop_coroutine() };

    if (task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    if (!task.await_suspend(firstHandle))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_suspend(handle), std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_suspend() used) only once."));
}

TEST_CASE("task<T>.await_resume() returns co_returned value")
{
    // Arrange
    constexpr std::string_view expected{ "expected" };
    std::unique_ptr<std::string_view> value{ std::make_unique<std::string_view>(expected) };
    async::task<std::unique_ptr<std::string_view>> task{ task_value_co_return(std::move(value)) };

    // Act & Assert
    std::unique_ptr<std::string_view> actual{ task.await_resume() };
    REQUIRE(expected == *actual);
}

TEST_CASE("task<T>.await_resume() throws when coroutine throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    async::task<int> task{ task_value_co_return_co_await(
        awaitable_value_throws<int>{ std::make_exception_ptr(expected) }) };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task<T>.await_resume() throws when called before completion")
{
    // Arrange
    async::task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };

    if (task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error,
        Catch::Matchers::Message("task<T>.await_resume() may not be called before await_ready() returns true."));
}

TEST_CASE("task<T>.await_resume() throws when called a second time")
{
    // Arrange
    constexpr int value{ 123 };
    async::task<int> task{ task_value_co_return(value) };

    if (!task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    if (task.await_resume() != value)
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_resume() used) only once."));
}

TEST_CASE("task<T&>.await_ready() returns true when task does not suspend")
{
    // Arrange
    int unusedStorage{ 123 };
    int& unusedValue{ unusedStorage };
    async::task<int&> task{ task_value_co_return<int&>(unusedValue) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(ready);
}

TEST_CASE("task<T&>.await_ready() returns false when task suspends")
{
    // Arrange
    async::task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(!ready);
}

TEST_CASE("task<T&>.await_suspend() returns true when task is suspended")
{
    // Arrange
    async::task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(suspended);
}

TEST_CASE("task<T&>.await_suspend() returns false when task is not suspended")
{
    // Arrange
    int unusedStorage{ 123 };
    int& unusedValue{ unusedStorage };
    async::task<int&> task{ task_value_co_return<int&>(unusedValue) };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(!suspended);
}

TEST_CASE("task<T&>.await_suspend() does not run continuation when task is suspended")
{
    // Arrange
    async::task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };
    async::details::atomic_acq_rel<bool> run{ false };
    auto continuation = [&run](async::awaitable_result<int&>) { run = true; };

    // Act
    async::awaitable_then(task, continuation);

    // Assert
    REQUIRE(!run);
}

TEST_CASE("task<T&>.await_suspend() runs continuation when task completes")
{
    // Arrange
    callback_thread callbackThread{};
    int unusedStorage{ 123 };
    int& unusedValue{ unusedStorage };
    async::task<int&> task{ task_value_co_return_co_await(
        suspend_to_paused_callback_thread_awaitable_value<int&>{ callbackThread, unusedValue }) };
    async::event_signal done{};
    auto continuation = [&done](async::awaitable_result<int&>) { done.set(); };
    async::awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task<T&>.await_suspend() does not run continuation before leaving coroutine scope")
{
    // Arrange
    bool scopeDestroyed{ false };
    bool scopeDestroyedDuringCompletion{};
    callback_thread callbackThread{};
    int unusedStorage{ 123 };
    int& unusedValue{ unusedStorage };
    async::task<int&> task{ task_value_co_return_co_await_with_scope(
        scopeDestroyed, suspend_to_paused_callback_thread_awaitable_value<int&>{ callbackThread, unusedValue }) };
    async::event_signal done{};
    auto continuation = [&scopeDestroyed, &scopeDestroyedDuringCompletion, &done](async::awaitable_result<int&>)
    {
        scopeDestroyedDuringCompletion = scopeDestroyed;
        done.set();
    };
    async::awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    done.wait_for_or_throw(std::chrono::seconds{ 1 });
    REQUIRE(scopeDestroyedDuringCompletion);
}

TEST_CASE("task<T&>.await_suspend() throws if another continuation is present")
{
    // Arrange
    async::task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };
    std::noop_coroutine_handle firstHandle{ std::noop_coroutine() };

    if (task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    if (!task.await_suspend(firstHandle))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_suspend(handle), std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_suspend() used) only once."));
}

TEST_CASE("task<T&>.await_resume() returns co_returned value")
{
    // Arrange
    int storage{ 123 };
    int& expected{ storage };
    async::task<int&> task{ task_value_co_return<int&>(expected) };

    // Act & Assert
    int& actual{ task.await_resume() };
    expected = 456; // ensure not a distinct copy
    REQUIRE(expected == actual);
}

TEST_CASE("task<T&>.await_resume() throws when coroutine throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    async::task<int&> task{ task_value_co_return_co_await(
        awaitable_value_throws<int&>{ std::make_exception_ptr(expected) }) };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task<T&>.await_resume() throws when called before completion")
{
    // Arrange
    async::task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };

    if (task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error,
        Catch::Matchers::Message("task<T>.await_resume() may not be called before await_ready() returns true."));
}

TEST_CASE("task<T&>.await_resume() throws when called a second time")
{
    // Arrange
    int storage{ 123 };
    int& value{ storage };
    async::task<int&> task{ task_value_co_return<int&>(value) };

    if (!task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    if (task.await_resume() != value)
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_resume() used) only once."));
}

TEST_CASE("task<T!has_default_ctor>.await_resume() returns co_returned value")
{
    // Arrange
    constexpr int expected{ 123 };
    async::task<no_default_constructor_move_only> task{ task_value_co_return(
        no_default_constructor_move_only{ expected }) };

    // Act
    no_default_constructor_move_only actual{ task.await_resume() };

    // Assert
    REQUIRE(expected == actual.get());
}
