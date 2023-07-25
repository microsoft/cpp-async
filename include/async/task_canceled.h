// © Microsoft Corporation. All rights reserved.

#pragma once

namespace async
{
    struct task_canceled : std::exception
    {
        [[nodiscard]] const char* what() const override { return "task canceled"; }
    };
}
