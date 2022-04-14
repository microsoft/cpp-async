// © Microsoft Corporation. All rights reserved.

#include <exception>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <catch2/catch.hpp>
#include "task.h"
#include "awaitable_then.h"
#include "awaitable_value_throws.h"
#include "awaitable_void_throws.h"
#include "callback_thread.h"
#include "event_signal.h"
#include "no_default_constructor_move_only.h"

inline task<void> task_void_co_return() { co_return; }

TEST_CASE("task<void>.await_ready() returns true when task does not suspend")
{
    // Arrange
    task<void> task{ task_void_co_return() };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(ready);
}

struct never_ready_awaitable_void final
{
    constexpr bool await_ready() const noexcept { return false; }

    constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}

    void await_resume() const
    {
        assert(false);
        throw std::runtime_error{ "This awaitable never resumes." };
    }
};

template<typename Awaitable>
inline task<void> task_void_co_await(Awaitable awaitable)
{
    co_await awaitable;
}

TEST_CASE("task<void>.await_ready() returns false when task suspends")
{
    // Arrange
    task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(!ready);
}

TEST_CASE("task<void>.await_suspend() returns true when task is suspended")
{
    // Arrange
    task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(suspended);
}

TEST_CASE("task<void>.await_suspend() returns false when task is not suspended")
{
    // Arrange
    task<void> task{ task_void_co_return() };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(!suspended);
}

TEST_CASE("task<void>.await_suspend() does not run continuation when task is suspended")
{
    // Arrange
    task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };
    atomic_acq_rel<bool> run{ false };
    auto continuation = [&run](awaitable_result<void>) { run = true; };

    // Act
    awaitable_then(task, continuation);

    // Assert
    REQUIRE(!run);
}

struct suspend_to_paused_callback_thread_awaitable_void final
{
    explicit suspend_to_paused_callback_thread_awaitable_void(callback_thread& thread) noexcept : m_thread{ thread } {}

    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) const noexcept { m_thread.callback(handle); }

    constexpr void await_resume() const noexcept {}

private:
    callback_thread& m_thread;
};

TEST_CASE("task<void>.await_suspend() runs continuation when task completes")
{
    // Arrange
    callback_thread callbackThread{};
    task<void> task{ task_void_co_await(suspend_to_paused_callback_thread_awaitable_void{ callbackThread }) };
    event_signal done{};
    auto continuation = [&done](awaitable_result<void>) { done.set(); };
    awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

struct scope_spy
{
    constexpr explicit scope_spy(bool& destroyed) noexcept : m_destroyed{ destroyed } {}

    ~scope_spy() noexcept { m_destroyed = true; }

private:
    bool& m_destroyed;
};

template<typename Awaitable>
task<void> task_void_co_return_co_await_with_scope(bool& scopeDestroyed, Awaitable awaitable)
{
    scope_spy destroyBeforeContinue{ scopeDestroyed };
    co_await awaitable;
    co_return;
}

TEST_CASE("task<void>.await_suspend() does not run continuation before leaving coroutine scope")
{
    // Arrange
    bool scopeDestroyed{ false };
    bool scopeDestroyedDuringCompletion{};
    callback_thread callbackThread{};
    task<void> task{ task_void_co_return_co_await_with_scope(
        scopeDestroyed, suspend_to_paused_callback_thread_awaitable_void{ callbackThread }) };
    event_signal done{};
    auto continuation = [&scopeDestroyed, &scopeDestroyedDuringCompletion, &done](awaitable_result<void>) {
        scopeDestroyedDuringCompletion = scopeDestroyed;
        done.set();
    };
    awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    done.wait_for_or_throw(std::chrono::seconds{ 1 });
    REQUIRE(scopeDestroyedDuringCompletion);
}

TEST_CASE("task<void>.await_suspend() throws if another continuation is present")
{
    // Arrange
    task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };
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
    REQUIRE_THROWS_MATCHES(
        task.await_suspend(handle),
        std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_suspend() used) only once."));
}

TEST_CASE("task<void>.await_resume() does not throw when task does not throw")
{
    // Arrange
    task<void> task{ task_void_co_return() };

    // Act & Assert
    REQUIRE_NOTHROW(task.await_resume());
}

TEST_CASE("task<void>.await_resume() throws when task throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    task<void> task{ task_void_co_await(awaitable_void_throws{ std::make_exception_ptr(expected) }) };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task<void>.await_resume() throws when called before completion")
{
    // Arrange
    task<void> task{ task_void_co_await(never_ready_awaitable_void{}) };

    if (task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        task.await_resume(),
        std::runtime_error,
        Catch::Matchers::Message("task<T>.await_resume() may not be called before await_ready() returns true."));
}

TEST_CASE("task<void>.await_resume() throws when called a second time")
{
    // Arrange
    task<void> task{ task_void_co_return() };

    if (!task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    task.await_resume();

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        task.await_resume(),
        std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_resume() used) only once."));
}

template<typename T>
task<T> task_value_co_return(T value)
{
    co_return value;
}

TEST_CASE("task<T>.await_ready() returns true when task does not suspend")
{
    // Arrange
    constexpr int unusedValue{ 123 };
    task<int> task{ task_value_co_return(unusedValue) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(ready);
}

template<typename T>
struct never_ready_awaitable_value final
{
    constexpr bool await_ready() const noexcept { return false; }

    constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}

    T await_resume() const
    {
        assert(false);
        throw std::runtime_error{ "This awaitable never resumes." };
    }
};

template<typename Awaitable>
auto task_value_co_return_co_await(Awaitable awaitable) -> task<decltype(awaitable.await_resume())>
{
    co_return co_await awaitable;
}

TEST_CASE("task<T>.await_ready() returns false when task suspends")
{
    // Arrange
    task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(!ready);
}

TEST_CASE("task<T>.await_suspend() returns true when task is suspended")
{
    // Arrange
    task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };
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
    task<int> task{ task_value_co_return(unusedValue) };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(!suspended);
}

TEST_CASE("task<T>.await_suspend() does not run continuation when task is suspended")
{
    // Arrange
    task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };
    atomic_acq_rel<bool> run{ false };
    auto continuation = [&run](awaitable_result<int>) { run = true; };

    // Act
    awaitable_then(task, continuation);

    // Assert
    REQUIRE(!run);
}

template<typename T>
struct suspend_to_paused_callback_thread_awaitable_value final
{
    explicit suspend_to_paused_callback_thread_awaitable_value(callback_thread& thread, T value) noexcept :
        m_thread{ thread }, m_value{ value }
    {}

    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle) const noexcept { m_thread.callback(handle); }

    constexpr T await_resume() const noexcept { return m_value; }

private:
    callback_thread& m_thread;
    T m_value;
};

TEST_CASE("task<T>.await_suspend() runs continuation when task completes")
{
    // Arrange
    callback_thread callbackThread{};
    constexpr int unusedValue{ 123 };
    task<int> task{ task_value_co_return_co_await(
        suspend_to_paused_callback_thread_awaitable_value{ callbackThread, unusedValue }) };
    event_signal done{};
    auto continuation = [&done](awaitable_result<int>) { done.set(); };
    awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

template<typename Awaitable>
auto task_value_co_return_co_await_with_scope(bool& scopeDestroyed, Awaitable awaitable)
    -> task<decltype(awaitable.await_resume())>
{
    scope_spy destroyBeforeContinue{ scopeDestroyed };
    co_return co_await awaitable;
}

TEST_CASE("task<T>.await_suspend() does not run continuation before leaving coroutine scope")
{
    // Arrange
    bool scopeDestroyed{ false };
    bool scopeDestroyedDuringCompletion{};
    callback_thread callbackThread{};
    constexpr int unusedValue{ 123 };
    task<int> task{ task_value_co_return_co_await_with_scope(
        scopeDestroyed, suspend_to_paused_callback_thread_awaitable_value{ callbackThread, unusedValue }) };
    event_signal done{};
    auto continuation = [&scopeDestroyed, &scopeDestroyedDuringCompletion, &done](awaitable_result<int>) {
        scopeDestroyedDuringCompletion = scopeDestroyed;
        done.set();
    };
    awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    done.wait_for_or_throw(std::chrono::seconds{ 1 });
    REQUIRE(scopeDestroyedDuringCompletion);
}

TEST_CASE("task<T>.await_suspend() throws if another continuation is present")
{
    // Arrange
    task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };
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
    REQUIRE_THROWS_MATCHES(
        task.await_suspend(handle),
        std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_suspend() used) only once."));
}

TEST_CASE("task<T>.await_resume() returns co_returned value")
{
    // Arrange
    constexpr std::string_view expected{ "expected" };
    std::unique_ptr<std::string_view> value{ std::make_unique<std::string_view>(expected) };
    task<std::unique_ptr<std::string_view>> task{ task_value_co_return(std::move(value)) };

    // Act & Assert
    std::unique_ptr<std::string_view> actual{ task.await_resume() };
    REQUIRE(expected == *actual);
}

TEST_CASE("task<T>.await_resume() throws when coroutine throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    task<int> task{ task_value_co_return_co_await(awaitable_value_throws<int>{ std::make_exception_ptr(expected) }) };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task<T>.await_resume() throws when called before completion")
{
    // Arrange
    task<int> task{ task_value_co_return_co_await(never_ready_awaitable_value<int>{}) };

    if (task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        task.await_resume(),
        std::runtime_error,
        Catch::Matchers::Message("task<T>.await_resume() may not be called before await_ready() returns true."));
}

TEST_CASE("task<T>.await_resume() throws when called a second time")
{
    // Arrange
    constexpr int value{ 123 };
    task<int> task{ task_value_co_return(value) };

    if (!task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    if (task.await_resume() != value)
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        task.await_resume(),
        std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_resume() used) only once."));
}

TEST_CASE("task<T&>.await_ready() returns true when task does not suspend")
{
    // Arrange
    int unusedStorage{ 123 };
    int& unusedValue{ unusedStorage };
    task<int&> task{ task_value_co_return<int&>(unusedValue) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(ready);
}

TEST_CASE("task<T&>.await_ready() returns false when task suspends")
{
    // Arrange
    task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };

    // Act
    bool ready{ task.await_ready() };

    // Assert
    REQUIRE(!ready);
}

TEST_CASE("task<T&>.await_suspend() returns true when task is suspended")
{
    // Arrange
    task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };
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
    task<int&> task{ task_value_co_return<int&>(unusedValue) };
    std::noop_coroutine_handle handle{ std::noop_coroutine() };

    // Act
    bool suspended{ task.await_suspend(handle) };

    // Assert
    REQUIRE(!suspended);
}

TEST_CASE("task<T&>.await_suspend() does not run continuation when task is suspended")
{
    // Arrange
    task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };
    atomic_acq_rel<bool> run{ false };
    auto continuation = [&run](awaitable_result<int&>) { run = true; };

    // Act
    awaitable_then(task, continuation);

    // Assert
    REQUIRE(!run);
}

TEST_CASE("task<T&>.await_suspend() runs continuation when task completes")
{
    // Arrange
    callback_thread callbackThread{};
    int unusedStorage{ 123 };
    int& unusedValue{ unusedStorage };
    task<int&> task{ task_value_co_return_co_await(
        suspend_to_paused_callback_thread_awaitable_value<int&>{ callbackThread, unusedValue }) };
    event_signal done{};
    auto continuation = [&done](awaitable_result<int&>) { done.set(); };
    awaitable_then(task, continuation);

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
    task<int&> task{ task_value_co_return_co_await_with_scope(
        scopeDestroyed, suspend_to_paused_callback_thread_awaitable_value<int&>{ callbackThread, unusedValue }) };
    event_signal done{};
    auto continuation = [&scopeDestroyed, &scopeDestroyedDuringCompletion, &done](awaitable_result<int&>) {
        scopeDestroyedDuringCompletion = scopeDestroyed;
        done.set();
    };
    awaitable_then(task, continuation);

    // Act
    callbackThread.resume();

    // Assert
    done.wait_for_or_throw(std::chrono::seconds{ 1 });
    REQUIRE(scopeDestroyedDuringCompletion);
}

TEST_CASE("task<T&>.await_suspend() throws if another continuation is present")
{
    // Arrange
    task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };
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
    REQUIRE_THROWS_MATCHES(
        task.await_suspend(handle),
        std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_suspend() used) only once."));
}
TEST_CASE("task<T&>.await_resume() returns co_returned value")
{
    // Arrange
    int storage{ 123 };
    int& expected{ storage };
    task<int&> task{ task_value_co_return<int&>(expected) };

    // Act & Assert
    int& actual{ task.await_resume() };
    expected = 456; // ensure not a distinct copy
    REQUIRE(expected == actual);
}

TEST_CASE("task<T&>.await_resume() throws when coroutine throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    task<int&> task{ task_value_co_return_co_await(awaitable_value_throws<int&>{ std::make_exception_ptr(expected) }) };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task<T&>.await_resume() throws when called before completion")
{
    // Arrange
    task<int&> task{ task_value_co_return_co_await(never_ready_awaitable_value<int&>{}) };

    if (task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        task.await_resume(),
        std::runtime_error,
        Catch::Matchers::Message("task<T>.await_resume() may not be called before await_ready() returns true."));
}

TEST_CASE("task<T&>.await_resume() throws when called a second time")
{
    // Arrange
    int storage{ 123 };
    int& value{ storage };
    task<int&> task{ task_value_co_return<int&>(value) };

    if (!task.await_ready())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    if (task.await_resume() != value)
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        task.await_resume(),
        std::runtime_error,
        Catch::Matchers::Message("task<T> may be co_awaited (or have await_resume() used) only once."));
}

TEST_CASE("task<T!has_default_ctor>.await_resume() returns co_returned value")
{
    // Arrange
    constexpr int expected{ 123 };
    task<no_default_constructor_move_only> task{ task_value_co_return(no_default_constructor_move_only{ expected }) };

    // Act
    no_default_constructor_move_only actual{ task.await_resume() };

    // Assert
    REQUIRE(expected == actual.get());
}
