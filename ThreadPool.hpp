#ifndef __THREAD_POOL_HPP__
    #define __THREAD_POOL_HPP__

#if __cplusplus >= 201703L

#ifndef __MULTIPLE_POOL_HPP__

#include "SafeQueue.hpp"

#include <string>
#include <exception> // std::exception
#include <vector> // std::vector
#include <memory> // std::shared_ptr
#include <functional> // std::function
#include <optional> // std::optional
#include <type_traits> // std::is_same & std::remove_reference

#include <atomic> // std::atomic
#include <mutex> // std::mutex
#include <condition_variable> // std::condition_variable
#include <future> // std::future
#include <thread> // std::thread

// avoid redefinition errors
namespace Gadgetry {
    class bad_submit : public std::exception {
        std::string message;
    public:
        bad_submit(std::string mes): message{std::move(mes)} {}
        virtual ~bad_submit() noexcept {}
        virtual const char* what() const noexcept { return message.c_str(); }
    };
} // namespace Gadgetry

#endif // __MULTIPLE_POOL_HPP__

namespace Gadgetry {
    class ThreadPool {
        using WorkItem = std::function<void()>;
        enum class ErrorLevel {
            ignore, preserve
        };
        struct Worker {
            ThreadPool *_belongs;
            std::size_t _id;

            Worker(ThreadPool *pool, std::size_t id): _belongs{pool}, _id{id} {}
            void operator()() {
                while (!_belongs->_shutdown) {
                    {
                        std::unique_lock<std::mutex> lock {_belongs->_cond_mtx};
                        _belongs->_thread_cond.wait(lock, [this]() {
                            return !this->_belongs->_task_list.empty() || this->_belongs->_shutdown;
                        });
                    }
                    auto tsk = _belongs->_task_list.dequeue();
                    if (tsk.has_value()) {
                        switch (_belongs->handler) {
                        case ThreadPool::ErrorLevel::ignore:
                            try { tsk.value()(); }
                            catch(...) {}
                            break;
                        case ThreadPool::ErrorLevel::preserve:
                        default:
                            try { tsk.value()(); }
                            catch(...) {
                                _belongs->_error_list.enqueue(std::current_exception());
                            }
                            break;
                        }
                    }
                }
            }
        };

        Gadgetry::SafeQueue<std::exception_ptr> _error_list;
        Gadgetry::SafeQueue<WorkItem> _task_list;
        std::vector<std::thread> _workers;
        std::mutex _cond_mtx;
        std::condition_variable _thread_cond;
        ThreadPool::ErrorLevel handler;
        std::atomic<bool> _shutdown, _stop_submit;

    public:
        static constexpr ErrorLevel ignore = ThreadPool::ErrorLevel::ignore;
        static constexpr ErrorLevel preserve = ThreadPool::ErrorLevel::preserve;

        explicit ThreadPool(
            const std::size_t size = std::thread::hardware_concurrency(),
            ThreadPool::ErrorLevel handle_type = ThreadPool::preserve
        )   : _workers{size}, _shutdown{false}, _stop_submit{false} {
            std::size_t i = 0;
            for (auto& thread : _workers)
                thread = std::thread(ThreadPool::Worker(this, i++));
        }
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;
        ~ThreadPool() { shutdown(); }

        std::size_t get_thread_count() const { return _workers.size(); }
        void rethrow_errors() {
            while (!_error_list.empty()) {
                auto exception = _error_list.dequeue();
                if (exception.has_value())
                    std::rethrow_exception(exception.value());
            }
        }
        void shutdown() {
            _stop_submit = true;
            while (!_task_list.empty()) // clear the task list
                _thread_cond.notify_one();
            _shutdown = true;
            _thread_cond.notify_all();
            for (auto& thread : _workers)
                if (thread.joinable()) thread.join();
        }
        template<typename F, typename... Args>
        auto submit(F&& tsk, Args&& ...args) -> std::future<std::invoke_result_t<F, Args...>> {
            if (_stop_submit) throw Gadgetry::bad_submit{"submit tasks to a closed thread pool"};

            using Ret = std::invoke_result_t<F, Args...>;
            std::function<Ret()> func = std::bind(tsk, std::forward<Args>(args)...);

            auto task_ptr = std::make_shared<std::packaged_task<Ret()>>(func);
            WorkItem warpper = [task_ptr]() { (*task_ptr)(); };

            _task_list.enqueue(warpper);
            _thread_cond.notify_one();
            return task_ptr->get_future();
        }
    };
} // namespace Gadgetry

#else
#error must have C++17 support
#endif // __cplusplus >= 201703L

#endif // __THREAD_POOL_HPP__
