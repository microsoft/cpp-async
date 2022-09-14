// © Microsoft Corporation. All rights reserved.

#include <catch2/catch.hpp>
#include "event_signal.h"
#include "task_completion_source.h"
#include "no_default_constructor_move_only.h"
#include "simplejthread.h"
#include "task.h"

TEST_CASE("task_completion_source<void>.task() starts as not ready")
{
    // Arrange
    async::task_completion_source<void> promise{};

    // Act
    async::task<void> task{ promise.task() };

    // Assert
    REQUIRE(!task.await_ready());
}

TEST_CASE("task_completion_source<void>.set_value() makes its task ready")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };

    // Act
    promise.set_value();

    // Assert
    REQUIRE(task.await_ready());
}

static async::task<void> co_await_void_finally_set_signal(async::task<void>&& awaitable, async::event_signal& done)
{
    try
    {
        co_await std::move(awaitable);
    }
    catch (...)
    {
    }

    done.set();
}

TEST_CASE("task_completion_source<void>.set_value() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::event_signal done{};
    co_await_void_finally_set_signal(promise.task(), done);

    // Act
    promise.set_value();

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<void>.set_value() makes its task not throw on await_resume().")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };

    // Act
    promise.set_value();

    // Assert
    REQUIRE_NOTHROW(task.await_resume());
}

TEST_CASE("task_completion_source<void>.set_value() throws if called a second time.")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    promise.set_value();

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        promise.set_value(),
        std::runtime_error,
        Catch::Matchers::Message("The task_completion_source<T> has already been completed."));
}

TEST_CASE("task_completion_source<void>.try_set_value() returns true when not yet completed")
{
    // Arrange
    async::task_completion_source<void> promise{};

    // Act
    bool result{ promise.try_set_value() };

    // Assert
    REQUIRE(result);
}

TEST_CASE("task_completion_source<void>.try_set_value() makes its task ready")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };

    // Act
    if (!promise.try_set_value())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<void>.try_set_value() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::event_signal done{};
    co_await_void_finally_set_signal(promise.task(), done);

    // Act
    if (!promise.try_set_value())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<void>.try_set_value() makes its task not throw on await_resume().")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };

    // Act
    if (!promise.try_set_value())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE_NOTHROW(task.await_resume());
}

TEST_CASE("task_completion_source<void>.try_set_value() returns false if called a second time.")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    if (!promise.try_set_value())
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act
    bool result{ promise.try_set_value() };

    // Assert
    REQUIRE(!result);
}

TEST_CASE("task_completion_source<void>.set_exception() throws if the exception_ptr is empty")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    std::exception_ptr empty{};

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        promise.set_exception(empty),
        std::invalid_argument,
        Catch::Matchers::Message("The exception_ptr must not be empty."));
}

TEST_CASE("task_completion_source<void>.set_exception() makes its task ready")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    std::exception_ptr unused{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    promise.set_exception(unused);

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<void>.set_exception() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::event_signal done{};
    co_await_void_finally_set_signal(promise.task(), done);
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    promise.set_exception(exception);

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<void>.set_exception() makes its task throw on await_resume().")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    std::runtime_error expected{ "expected" };
    std::exception_ptr exception{ std::make_exception_ptr(expected) };

    // Act
    promise.set_exception(exception);

    // Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task_completion_source<void>.set_exception() throws if called a second time.")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };
    promise.set_exception(exception);

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        promise.set_exception(exception),
        std::runtime_error,
        Catch::Matchers::Message("The task_completion_source<T> has already been completed."));
}

TEST_CASE("task_completion_source<void>.try_set_exception() returns false if the exception_ptr is empty")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    std::exception_ptr empty{};

    // Act
    bool result{ promise.try_set_exception(empty) };

    // Assert
    REQUIRE(!result);
}

TEST_CASE("task_completion_source<void>.try_set_exception() returns true when not yet completed")
{
    // Arrange
    async::task_completion_source<void> promise{};
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    bool result{ promise.try_set_exception(exception) };

    // Assert
    REQUIRE(result);
}

TEST_CASE("task_completion_source<void>.try_set_exception() makes its task ready")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<void>.try_set_exception() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::event_signal done{};
    co_await_void_finally_set_signal(promise.task(), done);
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<void>.try_set_exception() makes its task throw on await_resume().")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    std::runtime_error expected{ "expected" };
    std::exception_ptr exception{ std::make_exception_ptr(expected) };

    // Act
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task_completion_source<void>.try_set_exception() returns false if called a second time.")
{
    // Arrange
    async::task_completion_source<void> promise{};
    async::task<void> task{ promise.task() };
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act
    bool result{ promise.try_set_exception(exception) };

    // Assert
    REQUIRE(!result);
}

TEST_CASE("task_completion_source<T>.task() starts as not ready")
{
    // Arrange
    async::task_completion_source<int> promise{};

    // Act
    async::task<int> task{ promise.task() };

    // Assert
    REQUIRE(!task.await_ready());
}

TEST_CASE("task_completion_source<T>.set_value() makes its task ready")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    constexpr int value{ 123 };

    // Act
    promise.set_value(value);

    // Assert
    REQUIRE(task.await_ready());
}

template<typename T>
static async::task<void> co_await_value_finally_set_signal(async::task<T>&& awaitable, async::event_signal& done)
{
    try
    {
        (void)(co_await std::move(awaitable));
    }
    catch (...)
    {
    }

    done.set();
}

TEST_CASE("task_completion_source<T>.set_value() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::event_signal done{};
    co_await_value_finally_set_signal(promise.task(), done);
    constexpr int value{ 123 };

    // Act
    promise.set_value(value);

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<T>.set_value() makes its task return that value on await_resume().")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    constexpr int expected{ 123 };

    // Act
    promise.set_value(expected);

    // Assert
    REQUIRE(task.await_resume() == expected);
}

TEST_CASE("task_completion_source<T>.set_value() throws if called a second time.")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    constexpr int value{ 123 };
    promise.set_value(value);

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        promise.set_value(value),
        std::runtime_error,
        Catch::Matchers::Message("The task_completion_source<T> has already been completed."));
}

TEST_CASE("task_completion_source<T>.try_set_value() returns true when not yet completed")
{
    // Arrange
    async::task_completion_source<int> promise{};
    constexpr int value{ 123 };

    // Act
    bool result{ promise.try_set_value(value) };

    // Assert
    REQUIRE(result);
}

TEST_CASE("task_completion_source<T>.try_set_value() makes its task ready")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    constexpr int value{ 123 };

    // Act
    if (!promise.try_set_value(value))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<T>.try_set_value() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::event_signal done{};
    co_await_value_finally_set_signal(promise.task(), done);
    constexpr int value{ 123 };

    // Act
    if (!promise.try_set_value(value))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<T>.try_set_value() makes its task return that value on await_resume().")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    constexpr int expected{ 123 };

    // Act
    if (!promise.try_set_value(expected))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(task.await_resume() == expected);
}

TEST_CASE("task_completion_source<T>.try_set_value() returns false if called a second time.")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    constexpr int value{ 123 };
    if (!promise.try_set_value(value))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act
    bool result{ promise.try_set_value(value) };

    // Assert
    REQUIRE(!result);
}

TEST_CASE("task_completion_source<T>.set_exception() throws if the exception_ptr is empty")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    std::exception_ptr empty{};

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        promise.set_exception(empty),
        std::invalid_argument,
        Catch::Matchers::Message("The exception_ptr must not be empty."));
}

TEST_CASE("task_completion_source<T>.set_exception() makes its task ready")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    std::exception_ptr unused{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    promise.set_exception(unused);

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<T>.set_exception() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::event_signal done{};
    co_await_value_finally_set_signal(promise.task(), done);
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    promise.set_exception(exception);

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<T>.set_exception() makes its task throw on await_resume().")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    std::runtime_error expected{ "expected" };
    std::exception_ptr exception{ std::make_exception_ptr(expected) };

    // Act
    promise.set_exception(exception);

    // Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task_completion_source<T>.set_exception() throws if called a second time.")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };
    promise.set_exception(exception);

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        promise.set_exception(exception),
        std::runtime_error,
        Catch::Matchers::Message("The task_completion_source<T> has already been completed."));
}

TEST_CASE("task_completion_source<T>.try_set_exception() returns false if the exception_ptr is empty")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    std::exception_ptr empty{};

    // Act
    bool result{ promise.try_set_exception(empty) };

    // Assert
    REQUIRE(!result);
}

TEST_CASE("task_completion_source<T>.try_set_exception() returns true when not yet completed")
{
    // Arrange
    async::task_completion_source<int> promise{};
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    bool result{ promise.try_set_exception(exception) };

    // Assert
    REQUIRE(result);
}

TEST_CASE("task_completion_source<T>.try_set_exception() makes its task ready")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<T>.try_set_exception() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::event_signal done{};
    co_await_value_finally_set_signal(promise.task(), done);
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<T>.try_set_exception() makes its task throw on await_resume().")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    std::runtime_error expected{ "expected" };
    std::exception_ptr exception{ std::make_exception_ptr(expected) };

    // Act
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task_completion_source<T>.try_set_exception() returns false if called a second time.")
{
    // Arrange
    async::task_completion_source<int> promise{};
    async::task<int> task{ promise.task() };
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act
    bool result{ promise.try_set_exception(exception) };

    // Assert
    REQUIRE(!result);
}

TEST_CASE("task_completion_source<T&>.task() starts as not ready")
{
    // Arrange
    async::task_completion_source<int&> promise{};

    // Act
    async::task<int&> task{ promise.task() };

    // Assert
    REQUIRE(!task.await_ready());
}

TEST_CASE("task_completion_source<T&>.set_value() makes its task ready")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    int storage{ 123 };
    int& value{ storage };

    // Act
    promise.set_value(value);

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<T&>.set_value() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::event_signal done{};
    co_await_value_finally_set_signal(promise.task(), done);
    int storage{ 123 };
    int& value{ storage };

    // Act
    promise.set_value(value);

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<T&>.set_value() makes its task return that value on await_resume().")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    int storage{ 123 };
    int& expected{ storage };

    // Act
    promise.set_value(expected);

    // Assert
    int& actual{ task.await_resume() };
    expected = 456; // ensure not a distinct copy
    REQUIRE(expected == actual);
}

TEST_CASE("task_completion_source<T&>.set_value() throws if called a second time.")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    int storage{ 123 };
    int& value{ storage };
    promise.set_value(value);

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        promise.set_value(value),
        std::runtime_error,
        Catch::Matchers::Message("The task_completion_source<T> has already been completed."));
}

TEST_CASE("task_completion_source<T&>.try_set_value() returns true when not yet completed")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    int storage{ 123 };
    int& value{ storage };

    // Act
    bool result{ promise.try_set_value(value) };

    // Assert
    REQUIRE(result);
}

TEST_CASE("task_completion_source<T&>.try_set_value() makes its task ready")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    int storage{ 123 };
    int& value{ storage };

    // Act
    if (!promise.try_set_value(value))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<T&>.try_set_value() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::event_signal done{};
    co_await_value_finally_set_signal(promise.task(), done);
    int storage{ 123 };
    int& value{ storage };

    // Act
    if (!promise.try_set_value(value))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<T&>.try_set_value() makes its task return that value on await_resume().")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    int storage{ 123 };
    int& expected{ storage };

    // Act
    if (!promise.try_set_value(expected))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    int& actual{ task.await_resume() };
    expected = 456; // ensure not a distinct copy
    REQUIRE(expected == actual);
}

TEST_CASE("task_completion_source<T&>.try_set_value() returns false if called a second time.")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    int storage{ 123 };
    int& value{ storage };
    if (!promise.try_set_value(value))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act
    bool result{ promise.try_set_value(value) };

    // Assert
    REQUIRE(!result);
}

TEST_CASE("task_completion_source<T&>.set_exception() throws if the exception_ptr is empty")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    std::exception_ptr empty{};

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        promise.set_exception(empty),
        std::invalid_argument,
        Catch::Matchers::Message("The exception_ptr must not be empty."));
}

TEST_CASE("task_completion_source<T&>.set_exception() makes its task ready")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    std::exception_ptr unused{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    promise.set_exception(unused);

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<T&>.set_exception() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::event_signal done{};
    co_await_value_finally_set_signal(promise.task(), done);
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    promise.set_exception(exception);

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<T&>.set_exception() makes its task throw on await_resume().")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    std::runtime_error expected{ "expected" };
    std::exception_ptr exception{ std::make_exception_ptr(expected) };

    // Act
    promise.set_exception(exception);

    // Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task_completion_source<T&>.set_exception() throws if called a second time.")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };
    promise.set_exception(exception);

    // Act & Assert
    REQUIRE_THROWS_MATCHES(
        promise.set_exception(exception),
        std::runtime_error,
        Catch::Matchers::Message("The task_completion_source<T> has already been completed."));
}

TEST_CASE("task_completion_source<T&>.try_set_exception() returns false if the exception_ptr is empty")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    std::exception_ptr empty{};

    // Act
    bool result{ promise.try_set_exception(empty) };

    // Assert
    REQUIRE(!result);
}

TEST_CASE("task_completion_source<T&>.try_set_exception() returns true when not yet completed")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    bool result{ promise.try_set_exception(exception) };

    // Assert
    REQUIRE(result);
}

TEST_CASE("task_completion_source<T&>.try_set_exception() makes its task ready")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(task.await_ready());
}

TEST_CASE("task_completion_source<T&>.try_set_exception() resumes a coroutine suspended on its task")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::event_signal done{};
    co_await_value_finally_set_signal(promise.task(), done);
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };

    // Act
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE(done.wait_for(std::chrono::seconds{ 1 }));
}

TEST_CASE("task_completion_source<T&>.try_set_exception() makes its task throw on await_resume().")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    std::runtime_error expected{ "expected" };
    std::exception_ptr exception{ std::make_exception_ptr(expected) };

    // Act
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Assert
    REQUIRE_THROWS_MATCHES(task.await_resume(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("task_completion_source<T&>.try_set_exception() returns false if called a second time.")
{
    // Arrange
    async::task_completion_source<int&> promise{};
    async::task<int&> task{ promise.task() };
    std::exception_ptr exception{ std::make_exception_ptr(std::runtime_error{ "" }) };
    if (!promise.try_set_exception(exception))
    {
        throw std::runtime_error{ "Precondition failed." };
    }

    // Act
    bool result{ promise.try_set_exception(exception) };

    // Assert
    REQUIRE(!result);
}

TEST_CASE("task_completion_source<T!copyable>.set_value() makes its task return that value on await_resume().")
{
    // Arrange
    constexpr std::string_view expected{ "expected" };
    async::task_completion_source<std::unique_ptr<std::string_view>> promise{};
    async::task<std::unique_ptr<std::string_view>> task{ promise.task() };

    // Act
    promise.set_value(std::make_unique<std::string_view>(expected));

    // Assert
    REQUIRE(*task.await_resume() == expected);
}

TEST_CASE("task_completion_source<T!has_default_ctor>.set_value() makes its task return that value on await_resume().")
{
    // Arrange
    constexpr int expected{ 123 };
    async::task_completion_source<no_default_constructor_move_only> promise{};
    async::task<no_default_constructor_move_only> task{ promise.task() };

    // Act
    promise.set_value(no_default_constructor_move_only{ expected });

    // Assert
    REQUIRE(task.await_resume().get() == expected);
}

struct move_only_signal_and_black_on_move final
{
    explicit move_only_signal_and_black_on_move(
        async::event_signal& moving,
        async::event_signal& resume,
        async::event_signal& done) :
        m_moving{ moving }, m_resume{ resume }, m_done{ done }
    {}

    move_only_signal_and_black_on_move(const move_only_signal_and_black_on_move&) = delete;

    move_only_signal_and_black_on_move(move_only_signal_and_black_on_move&& other) noexcept :
        m_moving{ other.m_moving }, m_resume{ other.m_resume }, m_done{ other.m_done }
    {
        m_moving.set();
        m_resume.wait_for_or_throw(std::chrono::seconds{ 1 });
        m_done.set();
    }

    ~move_only_signal_and_black_on_move() noexcept = default;

    move_only_signal_and_black_on_move& operator=(const move_only_signal_and_black_on_move&) = delete;

    move_only_signal_and_black_on_move& operator=(move_only_signal_and_black_on_move&& other) noexcept = delete;

private:
    async::event_signal& m_moving;
    const async::event_signal& m_resume;
    async::event_signal& m_done;
};

TEST_CASE("task_completion_source<T>.try_set_value() returns false when another thread is partway through setting.")
{
    // Arrange
    async::task_completion_source<move_only_signal_and_black_on_move> promise{};
    async::task<move_only_signal_and_black_on_move> task{ promise.task() };
    async::event_signal startedMove{};
    async::event_signal resumeMove{};
    async::event_signal moveDone{};
    simplejthread setValueThread{ [&promise, &startedMove, &resumeMove, &moveDone]() {
        promise.set_value(move_only_signal_and_black_on_move{ startedMove, resumeMove, moveDone });
    } };

    async::event_signal ignore1{};
    async::event_signal doNotBlock{};
    async::event_signal ignore2{};
    doNotBlock.set();
    startedMove.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Act
    bool result{ promise.try_set_value(move_only_signal_and_black_on_move{ ignore1, doNotBlock, ignore2 }) };

    // Assert
    resumeMove.set();
    moveDone.wait_for_or_throw(std::chrono::seconds{ 1 });
    REQUIRE(!result);
}
