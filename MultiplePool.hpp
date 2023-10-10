#ifndef __MULTIPLE_POOL_HPP__
    #define __MULTIPLE_POOL_HPP__

#if __cplusplus >= 201703L

#ifndef __THREAD_POOL_HPP__

#include "SafeQueue.hpp"

#include <string> // std::string
#include <exception> // std::exception
#include <vector> // std::vector
#include <memory> // std::shared_ptr
#include <functional> // std::function
#include <optional> // std::optional C++17
#include <type_traits> // std::is_same & std::remove_reference

#include <atomic> // std::atomic
#include <mutex> // std::mutex
#include <condition_variable> // std::condition_variable
#include <future> // std::future
#include <thread> // std::thread

// avoid redefinition errors
namespace Gadgetry {
    class bad_thread_id : public std::exception {
        std::string message;
    public:
        bad_thread_id(std::string mes): message {std::move(mes)} {}
        virtual ~bad_thread_id() noexcept {}
        virtual const char* what() const noexcept { return message.c_str(); }
    };
    class bad_submit : public std::exception {
        std::string message;
    public:
        bad_submit(std::string mes): message {std::move(mes)} {}
        virtual ~bad_submit() noexcept {}
        virtual const char* what() const noexcept { return message.c_str(); }
    };
} // namespace Gadgetry

#endif // __THREAD_POOL_HPP__

namespace Gadgetry {
    class MultiplePool {
        using WorkItem = std::function<void()>;
        enum class ErrorLevel {
            ignore, preserve
        };
        struct Worker {
            MultiplePool *_belongs;
            std::size_t _id;

            Worker(MultiplePool *pool, std::size_t id): _belongs {pool}, _id {id} {}
            void operator()() {
                while (!_belongs->_shutdown) {
                    {
                        std::unique_lock<std::mutex> lock {_belongs->_cond_mtx};
                        _belongs->_thread_cond.wait(lock, [this]() {
                            return !this->_belongs->_task_list[_id].empty() || this->_belongs->_shutdown;
                        });
                    }
                    auto tsk = _belongs->_task_list[_id].dequeue();
                    if (tsk.has_value()) {
                        switch (_belongs->handler) {
                        case MultiplePool::ErrorLevel::ignore:
                            try {
                                tsk.value()();
                            } catch (...) {}
                            break;
                        case MultiplePool::ErrorLevel::preserve:
                        default:
                            try {
                                tsk.value()();
                            } catch (...) {
                                _belongs->_error_list.enqueue(std::current_exception());
                            }
                            break;
                        }
                    }
                }
            }
        };

        Gadgetry::SafeQueue<std::exception_ptr> _error_list;
        std::vector<Gadgetry::SafeQueue<WorkItem>> _task_list;
        std::vector<std::thread> _workers;
        std::mutex _cond_mtx;
        std::condition_variable _thread_cond;
        MultiplePool::ErrorLevel handler;
        std::atomic<bool> _shutdown, _stop_submit;

        size_t dispatch() const {
            std::size_t order = 0;
            std::size_t min_size = ~static_cast<std::size_t>(0);
            for (std::size_t i=0; i<_task_list.size(); ++i) {
                if (_task_list[i].empty()) return i;
                else if (_task_list[i].size() <= min_size) {
                    min_size = _task_list[i].size();
                    order = i;
                }
            }
            return order;
        }

    public:
        static constexpr MultiplePool::ErrorLevel ignore = MultiplePool::ErrorLevel::ignore;
        static constexpr MultiplePool::ErrorLevel preserve = MultiplePool::ErrorLevel::preserve;

        explicit MultiplePool(
            const std::size_t size,
            MultiplePool::ErrorLevel handle_type
        ): _task_list {size}, _workers {size}, handler {handle_type}, _shutdown {false}, _stop_submit {false} {
            std::size_t i = 0; // thread ID starts from zero
            for (auto& thread : _workers)
                thread = std::thread(MultiplePool::Worker(this, i++));
        }
        explicit MultiplePool(const std::size_t size)
            : MultiplePool(size, MultiplePool::preserve) {} // only initialize size
        explicit MultiplePool(MultiplePool::ErrorLevel handle_type) // only initialize ErrorLevel
            : MultiplePool(std::thread::hardware_concurrency(), MultiplePool::preserve) {}
        MultiplePool(): MultiplePool(std::thread::hardware_concurrency(), MultiplePool::preserve) {} // default

        MultiplePool(const MultiplePool&) = delete;
        MultiplePool(MultiplePool&&) = delete;
        MultiplePool& operator=(const MultiplePool&) = delete;
        MultiplePool& operator=(MultiplePool&&) = delete;
        ~MultiplePool() { shutdown(); }

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
            for (const auto& list : _task_list)
                while (!list.empty()) // clear the task list
                    _thread_cond.notify_one();
            _shutdown = true;
            _thread_cond.notify_all();
            for (auto& thread : _workers)
                if (thread.joinable()) thread.join();
        }

        template<typename F, typename... Args>
        auto submit(F&& tsk, Args&& ...args)
            -> std::future<std::invoke_result_t<F, Args...>> {
            return submit(std::nullopt, std::forward<F>(tsk), std::forward<Args>(args)...);
        }
        template<typename F, typename... Args>
        auto submit(std::optional<std::size_t> thread_id, F&& tsk, Args&& ...args)
            -> std::future<std::invoke_result_t<F, Args...>> {
            if (_stop_submit) throw Gadgetry::bad_submit {"submit tasks to a closed thread pool"};

            using Ret = std::invoke_result_t<F, Args...>;
            std::function<Ret()> func = std::bind(std::forward<F>(tsk), std::forward<Args>(args)...);

            auto task_ptr = std::make_shared<std::packaged_task<Ret()>>(func);
            WorkItem warpper = [task_ptr]() { (*task_ptr)(); };

            if (!thread_id.has_value()) thread_id = dispatch();
            else if (thread_id.value() >= get_thread_count()) {
                throw Gadgetry::bad_thread_id {
                    "specified thread ID is out of range\nwhich specified ID is: '" +
                    std::to_string(thread_id.value()) +
                    "' , the number of threads is: '" +
                    std::to_string(get_thread_count()) +
                    "'"
                };
            }

            _task_list[thread_id.value()].enqueue(warpper);
            _thread_cond.notify_one();
            return task_ptr->get_future();
        }
    };
} // namespace Gadgetry

#else
#error must have C++17 support
#endif // __cplusplus >= 201703L

#endif // __MULTIPLE_POOL_HPP__
