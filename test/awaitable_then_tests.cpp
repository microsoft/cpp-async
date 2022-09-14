// © Microsoft Corporation. All rights reserved.

#include <catch2/catch.hpp>
#include <chrono>
#include <coroutine>
#include <memory>
#include <stdexcept>
#include <string_view>
#include "async/awaitable_then.h"
#include "async/atomic_acq_rel.h"
#include "async/event_signal.h"
#include "awaitable_reference_value.h"
#include "awaitable_value.h"
#include "awaitable_value_member_operator_co_await.h"
#include "awaitable_value_non_member_operator_co_await.h"
#include "awaitable_value_resume_spy.h"
#include "awaitable_value_throws.h"
#include "awaitable_void.h"
#include "awaitable_void_resume_spy.h"
#include "awaitable_void_throws.h"
#include "callback_thread.h"
#include "no_default_constructor_move_only.h"

TEST_CASE("awaitable_then<void> waits until resume from suspension to run completion")
{
    // Arrange
    callback_thread callbackThread{};
    async::details::atomic_acq_rel<bool> waited{ false };
    auto awaitable = awaitable_void_resume_spy(callbackThread, waited);
    async::details::atomic_acq_rel<bool> continued{ false };
    async::event_signal done{};
    auto continuation = [&callbackThread, &waited, &continued, &done](async::awaitable_result<void>) {
        if (callbackThread.is_this_thread() && waited.load())
        {
            continued = true;
        }

        done.set();
    };

    // Act
    async::awaitable_then(std::move(awaitable), continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Assert
    REQUIRE(continued.load());
}

namespace
{
    void rethrow_if_non_null(std::exception_ptr exception)
    {
        if (exception)
        {
            std::rethrow_exception(exception);
        }
    }
}

TEST_CASE("awaitable_then<void> awaitable result throws if awaitable throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    std::exception_ptr thrown{ std::make_exception_ptr(expected) };
    std::exception_ptr actual{};
    async::event_signal done{};
    auto continuation = [&actual, &done](async::awaitable_result<void> result) {
        try
        {
            result();
        }
        catch (...)
        {
            actual = std::current_exception();
        }

        done.set();
    };

    // Act
    async::awaitable_then(awaitable_void_throws{ thrown }, continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Act & Assert
    REQUIRE_THROWS_MATCHES(rethrow_if_non_null(actual), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("awaitable_then<void> awaitable result does not throw if awaitable does not throw")
{
    // Arrange
    std::exception_ptr actual{};
    async::event_signal done{};
    auto continuation = [&actual, &done](async::awaitable_result<void> result) {
        try
        {
            result();
        }
        catch (...)
        {
            actual = std::current_exception();
        }

        done.set();
    };

    // Act
    async::awaitable_then(awaitable_void{}, continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Act & Assert
    REQUIRE_NOTHROW(rethrow_if_non_null(actual));
}

TEST_CASE("awaitable_then<T> waits until resume from suspension to run completion")
{
    // Arrange
    callback_thread callbackThread{};
    async::details::atomic_acq_rel<bool> waited{ false };
    constexpr bool unusedValue{ true };
    auto awaitable = awaitable_value_resume_spy(callbackThread, waited, unusedValue);
    async::details::atomic_acq_rel<bool> continued{ false };
    async::event_signal done{};
    auto continuation = [&callbackThread, &waited, &continued, &done](async::awaitable_result<bool>) {
        if (callbackThread.is_this_thread() && waited.load())
        {
            continued = true;
        }

        done.set();
    };

    // Act
    async::awaitable_then(std::move(awaitable), continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Assert
    REQUIRE(continued.load());
}

TEST_CASE("awaitable_then<T> awaitable result throws if awaitable throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    std::exception_ptr thrown{ std::make_exception_ptr(expected) };
    std::exception_ptr actual{};
    async::event_signal done{};
    auto continuation = [&actual, &done](async::awaitable_result<bool> result) {
        try
        {
            result();
        }
        catch (...)
        {
            actual = std::current_exception();
        }

        done.set();
    };

    // Act
    async::awaitable_then(awaitable_value_throws<bool>{ thrown }, continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Act & Assert
    REQUIRE_THROWS_MATCHES(rethrow_if_non_null(actual), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("awaitable_then<T> awaitable result returns value")
{
    // Arrange
    constexpr std::string_view expected{ "expected" };
    std::unique_ptr<std::string_view> verifyMoveOnlyTypeWorksAtCompileTime{ std::make_unique<std::string_view>(
        expected) };
    std::unique_ptr<std::string_view> actual{};
    async::event_signal done{};
    auto awaitable = awaitable_value(std::move(verifyMoveOnlyTypeWorksAtCompileTime));
    auto continuation = [&actual, &done](async::awaitable_result<std::unique_ptr<std::string_view>> result) {
        actual = std::move(result());
        done.set();
    };

    // Act
    async::awaitable_then(std::move(awaitable), continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Assert
    REQUIRE(expected == *actual);
}

TEST_CASE("awaitable_then<T&> waits until resume from suspension to run completion")
{
    // Arrange
    callback_thread callbackThread{};
    async::details::atomic_acq_rel<bool> waited{ false };
    bool unusedValueStorage{ true };
    bool& unusedValue{ unusedValueStorage };
    auto awaitable = awaitable_value_resume_spy<bool&>(callbackThread, waited, unusedValue);
    async::details::atomic_acq_rel<bool> continued{ false };
    async::event_signal done{};
    auto continuation = [&callbackThread, &waited, &continued, &done](async::awaitable_result<bool&>) {
        if (callbackThread.is_this_thread() && waited.load())
        {
            continued = true;
        }

        done.set();
    };

    // Act
    async::awaitable_then(std::move(awaitable), continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Assert
    REQUIRE(continued.load());
}

TEST_CASE("awaitable_then<T&> awaitable result throws if awaitable throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    std::exception_ptr thrown{ std::make_exception_ptr(expected) };
    std::exception_ptr actual{};
    async::event_signal done{};
    auto continuation = [&actual, &done](async::awaitable_result<bool&> result) {
        try
        {
            result();
        }
        catch (...)
        {
            actual = std::current_exception();
        }

        done.set();
    };

    // Act
    async::awaitable_then(awaitable_value_throws<bool&>{ thrown }, continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Act & Assert
    REQUIRE_THROWS_MATCHES(rethrow_if_non_null(actual), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("awaitable_then<T&> awaitable result returns value")
{
    // Arrange
    int storage{ 123 };
    int& expected{ storage };
    int* actual{};
    async::event_signal done{};
    auto awaitable = awaitable_reference_value(expected);
    auto continuation = [&actual, &done](async::awaitable_result<int&> result) {
        actual = &result();
        done.set();
    };

    // Act
    async::awaitable_then(std::move(awaitable), continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Assert
    expected = 456; // ensure not a distinct copy
    REQUIRE(expected == *actual);
}

TEST_CASE("awaitable_then<T{!has_default_ctor}> awaitable result returns value")
{
    // Arrange
    constexpr int expected{ 123 };
    std::unique_ptr<no_default_constructor_move_only> actual{};
    async::event_signal done{};
    auto awaitable = awaitable_value(no_default_constructor_move_only{ expected });
    auto continuation = [&actual, &done](async::awaitable_result<no_default_constructor_move_only> result) {
        actual = std::make_unique<no_default_constructor_move_only>(result());
        done.set();
    };

    // Act
    async::awaitable_then(std::move(awaitable), continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Assert
    REQUIRE(expected == actual->get());
}

TEST_CASE("awaitable_then<T w/member operator co_await> awaitable result returns value")
{
    // Arrange
    constexpr int expected{ 123 };
    int actual{};
    async::event_signal done{};
    auto awaitable = awaitable_value_member_operator_co_await{ expected };
    auto continuation = [&actual, &done](async::awaitable_result<int> result) {
        actual = result();
        done.set();
    };

    // Act
    async::awaitable_then(std::move(awaitable), continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Assert
    REQUIRE(expected == actual);
}

TEST_CASE("awaitable_then<T w/non-member operator co_await> awaitable result returns awaitable value")
{
    // Arrange
    constexpr int expected{ 123 };
    int actual{};
    async::event_signal done{};
    auto awaitable = awaitable_value_non_member_operator_co_await{ expected };
    auto continuation = [&actual, &done](async::awaitable_result<int> result) {
        actual = result();
        done.set();
    };

    // Act
    async::awaitable_then(std::move(awaitable), continuation);
    done.wait_for_or_throw(std::chrono::seconds{ 1 });

    // Assert
    REQUIRE(expected == actual);
}
