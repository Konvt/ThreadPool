# ThreadPool-cpp17

**Contents**  
- [ThreadPool-cpp17](#threadpool-cpp17)
  - [Example usage](#example-usage)
  - [Create thread pool](#create-thread-pool)
  - [Exception handle](#exception-handle)
  - [Available interface](#available-interface)
- [ThreadPool-cpp17-zh\_cn](#threadpool-cpp17-zh_cn)
  - [使用例](#使用例)
  - [创建线程池](#创建线程池)
  - [异常处理](#异常处理)
  - [可用的接口](#可用的接口)

A straightforward implementation of a C++17 thread pool.

`ThreadPool` provides a thread pool with a shared task queue, allowing multiple threads to work on a single queue of tasks.

`MultiplePool` provides a thread pool where each thread has its own task queue. Task dispatching is achieved by calling private method `dispatch`.

Each thread pool offers similar interfaces, including two optional exception handling methods. And both of them depend on 'SafeQueue.hpp'.

## Example usage
```cpp
#include <iostream>
#include <mutex>
#include <chrono>
#include "MultiplePool.hpp"

std::mutex mtx;

void task(int id) {
    {
        std::lock_guard<std::mutex> lock {mtx};
        std::cout << "Task " << id << " started" << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    {
        std::lock_guard<std::mutex> lock {mtx};
        std::cout << "Task " << id << " completed" << std::endl;
    }
}

int main()
{
    Gadgetry::MultiplePool thread_pool;

    for (int i=1; i<=10; ++i) {
        thread_pool.submit(task, i);
    }

    thread_pool.shutdown();
    std::cout << "All tasks completed" << std::endl;
}
```

## Create thread pool
The constructor requires two optional default parameters.

The first one is used to specify the number of threads in the thread pool, while the second one is used to specify the exception handling approach of the thread pool. Once the parameters are specified, they cannot be changed.

When not explicitly specified, the second parameter is set to `MultiplePool::preserve`, and the first parameter is obtained by calling `std::thread::hardware_concurrency()`, which retrieves the maximum number of threads supported by the underlying hardware.
```cpp
explicit MultiplePool(
    const std::size_t size = std::thread::hardware_concurrency(),
    MultiplePool::ErrorLevel handle_type = MultiplePool::preserve
)   : _task_list{size}, _workers{size}, handler{handle_type}, _shutdown{false}, _stop_submit{false} {
    std::size_t i = 0; // thread ID starts from zero
    for (auto& thread : _workers)
        thread = std::thread(MultiplePool::Worker(this, i++));
}
```

## Exception handle
Both thread pools employ the same exception handling approach, using `MultiplePool` as an example:

If `MultiplePool::ignore` is used, it means that when a thread encounters an exception, it will be caught and discarded without any further action.

If `MultiplePool::preserve` is used, it means that when a thread encounters an exception, it will be caught and stored in a message queue. Later, all these exceptions can be rethrown by invoking `rethrow_errors` method.

It is safe to invoke `rethrow_errors` when no exceptions occurred.

## Available interface
- `std::size_t get_thread_count()`
  Returns the number of threads in the thread pool.

- `void rethrow_errors()`
  Rethrows the stored exceptions when the error level is `MultiplePool::preserve`.

- `void shutdown()`
  Shutdown the thread pool, stop accepting new tasks and wait for all tasks to complete.

- `template<typename F, typename... Args> auto submit(F&& tsk, Args&& ...args) -> std::future<std::invoke_result_t<F, Args...>>`
  Submits task to thread pool, an exception `bad_submit` will be throw if the thread pool is closed.

- `template<typename F, typename... Args> auto submit(std::optional<std::size_t> thread_id, F&& tsk, Args&& ...args) -> std::future<std::invoke_result_t<F, Args...>>`
  Submits the task to the specified thread for execution, an exception `bad_submit` will be throw if the thread pool is closed, another exception `bad_thread_id` will be throw if the specified thread ID is out of the number of threads. **This method only available in `MutiplePool`.**
- - -

# ThreadPool-cpp17-zh_cn
基于 C++17 的线程池简单实现。

`ThreadPool` 提供了一个带有共享任务队列的线程池，允许多个线程共享同一个任务队列.


`MultiplePool` 中每个线程都有自己的任务队列. 不显式指定线程 id 时任务派发由私有方法 `dispatch` 决定.

每个线程池都提供类似的接口，其中包括两个相同的可选异常处理方法. 两个线程池都依赖于 SafeQueue.hpp.

## 使用例
```cpp
#include <iostream>
#include <mutex>
#include <chrono>
#include "MultiplePool.hpp"

int main()
{
    Gadgetry::MultiplePool thread_pool;

    for (int i=1; i<=10; ++i) {
        thread_pool.submit([i]() {
            task(i);
        });
    }

    thread_pool.shutdown();
    std::cout << "All tasks completed" << std::endl;
}
```

## 创建线程池
构造函数接受两个可选的参数.

第一个参数用于指定线程池中的线程数，第二个参数用于指定线程池的异常处理方法. 参数一旦指定就不能更改.

当参数缺省时，第二个参数会被设置为 `MultiplePool::preserve`，第一个参数会被设置为 `std::thread::hardware_concurrency()` 的返回值，这个函数会返回底层支持的最大线程数.
```cpp
explicit MultiplePool(
    const std::size_t size = std::thread::hardware_concurrency(),
    MultiplePool::ErrorLevel handle_type = MultiplePool::preserve
)   : _task_list{size}, _workers{size}, handler{handle_type}, _shutdown{false}, _stop_submit{false} {
    std::size_t i = 0; // thread ID starts from zero
    for (auto& thread : _workers)
        thread = std::thread(MultiplePool::Worker(this, i++));
}
```

## 异常处理
两个线程池采用了相同的异常处理代码，这里以 `MultiplePool` 为例：

如果使用 `MultiplePool::ignore`，线程在遇到异常时会直接捕获并抛弃，假装无事发生.

如果使用 `MultiplePool::preserve`，那么被捕获的异常会被存储在消息队列中. 稍后可以通过调用 `rethrow_errors` 方法重新抛出这些异常.

没有异常发生时调用 `rethrow_errors` 是安全的.

## 可用的接口
- `std::size_t get_thread_count()`
  返回线程池中的线程数量.

- `void rethrow_errors()`
  如果异常处理等级是 `MultiplePool::preserve` 时会重新抛出所有捕获到的异常.

- `void shutdown()`
  关闭线程池，停止接受任务并等待所有任务处理完毕.

- `template<typename F, typename... Args> auto submit(F&& tsk, Args&& ...args) -> std::future<std::invoke_result_t<F, Args...>>`
  将任务提交给线程池，线程池已关闭时提交会抛出异常 `bad_submit`.

- `template<typename F, typename... Args> auto submit(std::optional<std::size_t> thread_id, F&& tsk, Args&& ...args) -> std::future<std::invoke_result_t<F, Args...>>`
  将任务提交给线程池中的指定线程，线程池已关闭时提交会抛出异常 `bad_submit`，指定的线程 id 超出了线程数量范围时会抛出异常 `bad_thread_id`. **这个方法仅适用于 `MutiplePool`.**
