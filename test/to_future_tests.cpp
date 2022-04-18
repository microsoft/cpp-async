// © Microsoft Corporation. All rights reserved.

#include <atomic>
#include <cassert>
#include <coroutine>
#include <future>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <catch2/catch.hpp>
#include "to_future.h"
#include "atomic_acq_rel.h"
#include "awaitable_reference_value.h"
#include "awaitable_value.h"
#include "awaitable_value_member_operator_co_await.h"
#include "awaitable_value_non_member_operator_co_await.h"
#include "awaitable_value_resume_spy.h"
#include "awaitable_value_throws.h"
#include "awaitable_void_resume_spy.h"
#include "awaitable_void_throws.h"
#include "callback_thread.h"

TEST_CASE("to_future(awaitable<void>).get() waits until resume from suspension")
{
    // Arrange
    callback_thread callbackThread{};
    async::details::atomic_acq_rel<bool> waited{ false };
    auto awaitable = awaitable_void_resume_spy(callbackThread, waited);
    std::future<void> future{ async::to_future(std::move(awaitable)) };

    // Act
    future.get();

    // Assert
    REQUIRE(waited.load());
}

TEST_CASE("to_future(awaitable<void>).get() throws if awaitable throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    std::exception_ptr thrown{ std::make_exception_ptr(expected) };
    std::exception_ptr actual{};
    std::future<void> future{ async::to_future(awaitable_void_throws{ thrown }) };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(future.get(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("to_future(awaitable<T>).get() waits until resume from suspension")
{
    // Arrange
    callback_thread callbackThread{};
    async::details::atomic_acq_rel<bool> waited{ false };
    constexpr bool unusedValue{ true };
    auto awaitable = awaitable_value_resume_spy(callbackThread, waited, unusedValue);
    std::future<bool> future{ async::to_future(std::move(awaitable)) };

    // Act
    future.get();

    // Assert
    REQUIRE(waited.load());
}

TEST_CASE("to_future(awaitable<T>).get() throws if awaitable throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    std::exception_ptr thrown{ std::make_exception_ptr(expected) };
    std::exception_ptr actual{};
    auto awaitable = awaitable_value_throws<bool>{ thrown };
    std::future<bool> future{ async::to_future(std::move(awaitable)) };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(future.get(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("to_future(awaitable<T>).get() returns awaitable value")
{
    // Arrange
    constexpr std::string_view expected{ "expected" };
    std::unique_ptr<std::string_view> verifyMoveOnlyTypeWorksAtCompileTime{ std::make_unique<std::string_view>(
        expected) };
    auto awaitable = awaitable_value(std::move(verifyMoveOnlyTypeWorksAtCompileTime));
    std::future<std::unique_ptr<std::string_view>> future{ async::to_future(std::move(awaitable)) };

    // Act
    std::unique_ptr<std::string_view> actual{ future.get() };

    // Assert
    REQUIRE(expected == *actual);
}

TEST_CASE("to_future(awaitable<T&>).get() waits until resume from suspension")
{
    // Arrange
    callback_thread callbackThread{};
    async::details::atomic_acq_rel<bool> waited{ false };
    bool unusedValueStorage{ true };
    bool& unusedValue{ unusedValueStorage };
    auto awaitable = awaitable_value_resume_spy<bool&>(callbackThread, waited, unusedValue);
    std::future<bool&> future{ async::to_future(std::move(awaitable)) };

    // Act
    future.get();

    // Assert
    REQUIRE(waited.load());
}

TEST_CASE("to_future(awaitable<T&>).get() throws if awaitable throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    std::exception_ptr thrown{ std::make_exception_ptr(expected) };
    std::exception_ptr actual{};
    std::future<bool&> future{ async::to_future(awaitable_value_throws<bool&>{ thrown }) };

    // Act & Assert
    REQUIRE_THROWS_MATCHES(future.get(), std::runtime_error, Catch::Matchers::Message(expected.what()));
}

TEST_CASE("to_future(awaitable<T&>).get() returns awaitable value")
{
    // Arrange
    int storage{ 123 };
    int& expected{ storage };
    auto awaitable = awaitable_reference_value(expected);
    std::future<int&> future{ async::to_future(std::move(awaitable)) };

    // Act
    int& actual{ future.get() };

    // Assert
    expected = 456; // ensure not a distinct copy
    REQUIRE(expected == actual);
}

// Note: the following test case would be here:
// async::to_future(awaitable<T!has_default_ctor>).get() returns awaitable value
// But support for such types is not possible, because std::promise does not support them.
// Specifically, event just using the following line results in a compliation failure:
// std::promise<no_default_constructor_move_only> promise{};
// (attempting to reference a deleted function - the default constructor).

TEST_CASE("to_future(awaitable<T w/member operator co_await>).get() returns awaitable value")
{
    // Arrange
    constexpr int expected{ 123 };
    std::future<int> future{ async::to_future(awaitable_value_member_operator_co_await{ expected }) };

    // Act
    int actual{ future.get() };

    // Assert
    REQUIRE(expected == actual);
}

TEST_CASE("to_future(awaitable<T w/non-member operator co_await>).get() returns awaitable value")
{
    // Arrange
    constexpr int expected{ 123 };
    std::future<int> future{ async::to_future(awaitable_value_non_member_operator_co_await{ expected }) };

    // Act
    int actual{ future.get() };

    // Assert
    REQUIRE(expected == actual);
}
