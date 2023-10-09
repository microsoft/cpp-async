# cpp-async

C++ 20 contains the core language support needed to make coroutines (async/await) possible, but it does not provide the
related types needed to write an actual coroutine or functions to consume coroutines in common ways. This repository
provides support types and functions to fill that gap, until such capabilities are made available as part of the C++
Standard Library.

# awaitable_get()

This function allows blocking the calling thread until an awaitable completes and returns the awaitable's value (or
void, as appropriate). It works with any awaitable type.

Example usage:
```c++
inline /*<some_awaitable_type>*/ run_async() { co_return; }

int main()
{
    async::awaitable_get(run_async());
}
```

```c++
inline /*<some_awaitable_type>*/ read_file_async()
{
    co_await /* some awaitable object */;
    co_return std::string{ "file contents" };
}

int main()
{
    std::string text{ async::awaitable_get(read_file_async()) };
    printf("%s\n", text.c_str());
}
```

# awaitable_then()

This function allows scheduling a single continuation to run when an awaitable completes. It provides a single argument
to the continuation, which, when invoked, returns the awaitable's (or void, as appropriate) if the awaitable succeeded
or throws if the awaitable failed. It works with any awaitable type.

If the provided continuation throws, await_then terminates the process. (A caller can avoid this behavior by surrounding
the continuation in a try/catch block.)

Example usage:
```c++
inline /*<some_awaitable_type>*/ read_file_async()
{
    co_await /* some awaitable object */;
    co_return std::string{ "file contents" };
}

int main()
{
    async::event_signal done{};
    async::awaitable_then(read_file_async(), [&done](async::awaitable_result<std::string> result)
        {
            printf("%s\n", result().c_str());
            done.set();
        });
    done.wait();
}
```

```c++
inline /*<some_awaitable_type>*/ read_file_async()
{
    co_await /* some awaitable object */;
    co_return std::string{ "file contents" };
}

std::future<std::string> read_file_future()
{
    std::shared_ptr<std::promise<std::string>> promise{ std::make_shared<std::promise<std::string>>() };
    async::awaitable_then(read_file_async(), [promise](async::awaitable_result<std::string> result)
        {
            try
            {
                promise->set_value(result());
            }
            catch (...)
            {
                promise->set_exception(std::current_exception());
            }
        });
    return promise->get_future();
}

int main()
{
    printf("%s\n", read_file_future().get().c_str());
}
```

# task<T>

This type is a coroutine return type; it allows writing a function as a coroutine (calling co_await/co_return).

task<T> supports the following return types:
* T is void
* T is a move-only type
* T is a type that does not have a default constructor
* T is a reference type

A caller may only co_await a task<T> once, and the result of the task is moved out when returning from co_await.

When working with the task<T> directly (rather than only passing it directly to co_await), a caller may cancel execution
of any code that would resume after the task completes by destructing the task. (Destructing the task does not stop the
task's coroutine from running, just any continuation that would run after the task's coroutine completes.) As a result,
an exception thrown from a task will be ignored if no caller consumes the task's result. (A caller can avoid this
behavior by consuming the task's result and handling the exception differently; for example by co_awaiting the task from
another coroutine and calling std::terminate when it throws.)

Internally, task<T> destroys the coroutine frame as soon as the coroutine completes; it does not wait until the task has
destructed (which would only be after executing any code that will run next). Both options here involve tradeoffs; the
alternative would avoid the need for an internal heap allocation when creating the task at the expense of keeping
(potentially large) coroutine frame memory in use longer.

Example usage:
```c++
inline async::task<void> do_async()
{
    /* do some work */
    co_await /* some awaitable object */;
    /* do some more work */
    co_return;
}
```

```c++
inline async::task<std::string> read_async()
{
    co_await /* some awaitable object */;
    co_return std::string{ "contents" };
}
```

```c++
inline async::task<std::unique_ptr<std::string>> return_move_only_async()
{
    co_return std::make_unique<std::string>("contents");
}

inline async::task<void> use_move_only_async()
{
    std::unique_ptr<std::string> text{ co_await return_move_only_async() };
    printf("%s\n", text->c_str());
}
```

```c++
inline async::task<int&> return_reference_type_async(int& a, int& b)
{
    int& larger{ a > b ? a : b };
    co_return larger;
}

inline async::task<void> use_reference_type_async()
{
    int first{ 3 };
    int second{ 6 };
    const int& larger{ co_await return_reference_type_async(first, second) };
    ++second;
    printf("%i\n", larger); // 7
}
```

```c++
inline async::task<void> fire_and_forget()
{
    /* do some work */

    co_await /* some awaitable object */;

    if (/* something bad happens*/)
    {
        throw std::runtime_error{ "fire_and_forget failed" };
    }

    co_return;
}

inline async::task<void> fire_and_forget_except_crash_on_failure()
{
    try
    {
        co_await fire_and_forget();
    }
    catch (...)
    {
        std::terminate();
    }
}

int main()
{
    {
        // uncomment one of the next lines (to ignore or crash on exception):
        //fire_and_forget();
        fire_and_forget_except_crash_on_failure();
    }

    std::this_thread::sleep_for(std::chrono::seconds{ 1 });
}
```

# task_canceled

For implementations that wish to treat cancellation as exceptional, this exception type can be thrown to indicate that
a task responded to a cancellation request rather than completing successfully.

Alternatives to exceptions include returning a sentinel value or using std::expected (C++23). For details, see
the Cancellation section below.

Example usage:
```c++
async::task<int> simulate_send_recv_async(int data, std::stop_token stopToken)
{
    // Simulate asynchronous networking calls and canceling them when stopToken is signaled.
    // (For example, a POSIX-based implementation might use APIs such as aio_write and aio_cancel.)
    async::event_signal stop{};
    std::stop_callback stopCallback{ stopToken, [&stop]() { stop.set(); } };

    if (stop.wait_for(std::chrono::seconds{ 5 }))
    {
        throw async::task_canceled{};
    }

    co_return data + 1;
}
```

# task_completion_source<T>

This type controls a task and allows it to be returned and completed separately. It is designed for producing an
awaitable type from a function that is not itself a coroutine.

The design of this type is intentionally similar to TaskCompletionSource<TResult> in .NET.

Example usage:
```c++
async::task<int> compute_async(std::thread& callbackThread)
{
    std::shared_ptr<async::task_completion_source<int>> promise{
        std::make_shared<async::task_completion_source<int>>() };
    // Capture the task_completion_source shared_ptr by value, or it will get destructed before the callbackThread runs.
    callbackThread = std::thread{ [promise]()
        {
            promise->set_value(123);
        }
    };
    printf("Returning the task now; will complete it later... ");
    return promise->task();
}

int main()
{
    std::thread callbackThread{};
    printf("[completed]\nThe task returned %i\n", async::awaitable_get(compute_async(callbackThread)));
    callbackThread.join();
}
```

# to_future()

This function produces a std::future<T> for an awaitable; it downgrades a C++ 20 awaitable/coroutine to a C++ 11
std::future<T> (for use with legacy code).

This function works with any awaitable type whose return value type can be used with std::promise<T>. Note that types
without default constructors are not supported for std::promise<T>

Example usage:
```c++
inline /*<some_awaitable_type>*/ read_file_async()
{
    co_await /* some awaitable object */;
    co_return std::string{ "file contents" };
}

// Convert to legacy C++ 11 async; for example, if needed to defer refactoring the caller.
inline std::future<std::string> read_file_future()
{
    return async::to_future(read_file_async());
}

int main()
{
    std::string text{ read_file_future().get() };
    printf("%s\n", text.c_str());
}
```

# Cancellation

A task can support cancellation requests using std::stop_token. Multiple options exist for communicating that a task has
been canceled, including returning a sentinel value, using std::expected and std::unexpected, or throwing an exception.

The following example code provides a harness for running a coroutine that cancels on SIGINT:

```c++
#include <atomic>
#include <csignal>
#include <iostream>
#include <stop_token>
#include <thread>
#include "async/awaitable_get.h"
#include "async/task.h"

namespace
{
    std::atomic_flag g_stop = ATOMIC_FLAG_INIT;

    void handle_interrupt(int signal) noexcept
    {
        if (signal != SIGINT)
        {
            return;
        }

        g_stop.test_and_set();
        g_stop.notify_all();
    }

    void monitor_interrupt(std::stop_source& stopSource)
    {
        g_stop.wait(false);
        stopSource.request_stop();
    }
}

// Add run_async from one of the specific cancellation options below.

int main(int argc, char* argv[])
{
    std::ignore = argc;
    std::ignore = argv;

    // stop_source for CTRL+C
    std::ignore = std::signal(SIGINT, handle_interrupt);
    std::stop_source interruptStopSource{};
    std::thread monitorInterrupt{ monitor_interrupt, std::ref(interruptStopSource) };

    try
    {
        async::awaitable_get(run_async(interruptStopSource.get_token()));

        g_stop.test_and_set();
        g_stop.notify_all();
        monitorInterrupt.join();
    }
    catch (...)
    {
        g_stop.test_and_set();
        g_stop.notify_all();
        monitorInterrupt.join();
    }
}
```

## Cancellation: Returning a Sentinel Value

A task can communicate cancellation by returning a sentinel value.

Example code (see the harness above to make a complete program):
```c++
async::task<int> simulate_send_recv_async(int data, std::stop_token stopToken)
{
    // Simulate calls to networking APIs, canceling them when stopToken is signaled.
    // (For example, a POSIX-based implementation might use APIs such as aio_write and aio_cancel.)
    async::event_signal stop{};
    std::stop_callback stopCallback{ stopToken, [&stop]() { stop.set(); } };

    if (stop.wait_for(std::chrono::seconds{ 5 }))
    {
        co_return -1;
    }

    co_return data + 1;
}

async::task<void> run_async(std::stop_token stopToken)
{
    std::cout << "simulating cancelable async network call (CTRL+C to cancel) . . .\n";
    std::cout.flush();

    int response{ co_await simulate_send_recv_async(123, stopToken) };

    if (response == -1)
    {
        std::cout << "canceled\n";
    }
    else
    {
        std::cout << "done\n";
    }

    co_return;
}
```

## Cancellation: std::expected and std::unexpected (C++23)

A task can communicate cancellation by returning std::unexpected.

Example code (see the harness above to make a complete program):
```c++
#include <expected>

enum class send_receive_error
{
    canceled
};

async::task<std::expected<int, send_receive_error>> simulate_send_recv_async(int data, std::stop_token stopToken)
{
    // Simulate asynchronous networking calls and canceling them when stopToken is signaled.
    // (For example, a POSIX-based implementation might use APIs such as aio_write and aio_cancel.)
    async::event_signal stop{};
    std::stop_callback stopCallback{ stopToken, [&stop]() { stop.set(); } };

    if (stop.wait_for(std::chrono::seconds{ 5 }))
    {
        co_return std::unexpected(send_receive_error::canceled);
    }

    co_return data + 1;
}

async::task<void> run_async(std::stop_token stopToken)
{
    std::cout << "simulating cancelable async network call (CTRL+C to cancel) . . .\n";
    std::cout.flush();

    std::expected<int, send_receive_error> result{ co_await simulate_send_recv_async(123, stopToken) };

    if (result)
    {
        std::cout << "done\n";
    }
    else
    {
        std::cout << "canceled\n";
    }

    co_return;
}
```

## Cancellation: Throwing an Exception

A task can communicate cancellation by throwing a task_canceled exception.

Example code (see the harness above to make a complete program):
```c++
#include "async/task_canceled.h"

async::task<int> simulate_send_recv_async(int data, std::stop_token stopToken)
{
    // Simulate asynchronous networking calls and canceling them when stopToken is signaled.
    // (For example, a POSIX-based implementation might use APIs such as aio_write and aio_cancel.)
    async::event_signal stop{};
    std::stop_callback stopCallback{ stopToken, [&stop]() { stop.set(); } };

    if (stop.wait_for(std::chrono::seconds{ 5 }))
    {
        throw async::task_canceled{};
    }

    co_return data + 1;
}

async::task<void> run_async(std::stop_token stopToken)
{
    std::cout << "simulating cancelable async network call (CTRL+C to cancel) . . .\n";
    std::cout.flush();

    try
    {
        std::ignore = co_await simulate_send_recv_async(123, stopToken);
        std::cout << "done\n";
    }
    catch (const async::task_canceled&)
    {
        std::cout << "canceled\n";
    }

    co_return;
}
```

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft
trademarks or logos is subject to and must follow
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.
