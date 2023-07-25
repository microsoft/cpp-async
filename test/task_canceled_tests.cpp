// © Microsoft Corporation. All rights reserved.

#include <exception>
#include <catch2/catch.hpp>
#include "async/task_canceled.h"

namespace
{
    void throw_task_canceled() { throw async::task_canceled{}; }
}

TEST_CASE("task_canceled inherits from std::exception")
{
    // Act & Assert
    REQUIRE_THROWS_AS(throw_task_canceled(), std::exception);
}

TEST_CASE("task_canceled has expected message")
{
    // Act & Assert
    REQUIRE_THROWS_WITH(throw_task_canceled(), "task canceled");
}
