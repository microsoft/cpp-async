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
    awaitable_get(run_async());
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
    std::string text{ awaitable_get(read_file_async()) };
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
    event_signal done{};
    awaitable_then(read_file_async(), [&done](awaitable_result<std::string> result)
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
    awaitable_then(read_file_async(), [promise](awaitable_result<std::string> result)
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
