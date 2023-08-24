// © Microsoft Corporation. All rights reserved.

#pragma once

namespace async
{
    struct task_canceled : std::exception
    {
        [[nodiscard]] const char* what() const noexcept override { return "task canceled"; }
    };
}
