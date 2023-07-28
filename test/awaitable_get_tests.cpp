// © Microsoft Corporation. All rights reserved.

#include <atomic>
#include <cassert>
#include <coroutine>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <catch2/catch.hpp>
#include "async/awaitable_get.h"
#include "async/atomic_acq_rel.h"
#include "awaitable_reference_value.h"
#include "awaitable_value.h"
#include "awaitable_value_member_operator_co_await.h"
#include "awaitable_value_non_member_operator_co_await.h"
#include "awaitable_value_resume_spy.h"
#include "awaitable_value_throws.h"
#include "awaitable_void_resume_spy.h"
#include "awaitable_void_throws.h"
#include "callback_thread.h"
#include "no_default_constructor_move_only.h"

TEST_CASE("awaitable_get<void> waits until resume from suspension")
{
    // Arrange
    callback_thread callbackThread{};
    async::details::atomic_acq_rel<bool> waited{ false };
    auto awaitable = awaitable_void_resume_spy(callbackThread, waited);

    // Act
    async::awaitable_get(std::move(awaitable));

    // Assert
    REQUIRE(waited.load());
}

TEST_CASE("awaitable_get<void> throws if awaitable throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    std::exception_ptr thrown{ std::make_exception_ptr(expected) };
    std::exception_ptr actual{};

    // Act & Assert
    REQUIRE_THROWS_MATCHES(async::awaitable_get(awaitable_void_throws{ thrown }), std::runtime_error,
        Catch::Matchers::Message(expected.what()));
}

TEST_CASE("awaitable_get<T> waits until resume from suspension")
{
    // Arrange
    callback_thread callbackThread{};
    async::details::atomic_acq_rel<bool> waited{ false };
    constexpr bool unusedValue{ true };
    auto awaitable = awaitable_value_resume_spy(callbackThread, waited, unusedValue);

    // Act
    async::awaitable_get(std::move(awaitable));

    // Assert
    REQUIRE(waited.load());
}

TEST_CASE("awaitable_get<T> throws if awaitable throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    std::exception_ptr thrown{ std::make_exception_ptr(expected) };
    std::exception_ptr actual{};

    // Act & Assert
    REQUIRE_THROWS_MATCHES(async::awaitable_get(awaitable_value_throws<bool>{ thrown }), std::runtime_error,
        Catch::Matchers::Message(expected.what()));
}

TEST_CASE("awaitable_get<T> returns awaitable value")
{
    // Arrange
    constexpr std::string_view expected{ "expected" };
    std::unique_ptr<std::string_view> verifyMoveOnlyTypeWorksAtCompileTime{ std::make_unique<std::string_view>(
        expected) };
    auto awaitable = awaitable_value(std::move(verifyMoveOnlyTypeWorksAtCompileTime));

    // Act
    std::unique_ptr<std::string_view> actual{ async::awaitable_get(std::move(awaitable)) };

    // Assert
    REQUIRE(expected == *actual);
}

TEST_CASE("awaitable_get<T&> waits until resume from suspension")
{
    // Arrange
    callback_thread callbackThread{};
    async::details::atomic_acq_rel<bool> waited{ false };
    bool unusedValueStorage{ true };
    bool& unusedValue{ unusedValueStorage };
    auto awaitable = awaitable_value_resume_spy<bool&>(callbackThread, waited, unusedValue);

    // Act
    async::awaitable_get(std::move(awaitable));

    // Assert
    REQUIRE(waited.load());
}

TEST_CASE("awaitable_get<T&> throws if awaitable throws")
{
    // Arrange
    std::runtime_error expected{ "expected" };
    std::exception_ptr thrown{ std::make_exception_ptr(expected) };
    std::exception_ptr actual{};

    // Act & Assert
    REQUIRE_THROWS_MATCHES(async::awaitable_get(awaitable_value_throws<bool&>{ thrown }), std::runtime_error,
        Catch::Matchers::Message(expected.what()));
}

TEST_CASE("awaitable_get<T&> returns awaitable value")
{
    // Arrange
    int storage{ 123 };
    int& expected{ storage };
    auto awaitable = awaitable_reference_value(expected);

    // Act
    int& actual{ async::awaitable_get(std::move(awaitable)) };

    // Assert
    expected = 456; // ensure not a distinct copy
    REQUIRE(expected == actual);
}

TEST_CASE("awaitable_get<T!has_default_ctor> returns awaitable value")
{
    // Arrange
    constexpr int expected{ 123 };
    auto awaitable = awaitable_value(no_default_constructor_move_only{ expected });

    // Act
    no_default_constructor_move_only actual{ async::awaitable_get(std::move(awaitable)) };

    // Assert
    REQUIRE(expected == actual.get());
}

TEST_CASE("awaitable_get<T w/member operator co_await> returns awaitable value")
{
    // Arrange
    constexpr int expected{ 123 };

    // Act
    int actual{ async::awaitable_get(awaitable_value_member_operator_co_await{ expected }) };

    // Assert
    REQUIRE(expected == actual);
}

TEST_CASE("awaitable_get<T w/non-member operator co_await> returns awaitable value")
{
    // Arrange
    constexpr int expected{ 123 };

    // Act
    int actual{ async::awaitable_get(awaitable_value_non_member_operator_co_await{ expected }) };

    // Assert
    REQUIRE(expected == actual);
}
