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
