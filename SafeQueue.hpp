#ifndef __SAFE_QUEUE_HPP__
    #define __SAFE_QUEUE_HPP__

#include <queue>
#include <mutex>
#include <shared_mutex>

#include <optional>

namespace Gadgetry {
    template<typename T>
    class SafeQueue {
        std::queue<T> _que;
        mutable std::shared_mutex _mtx; // reader-writer problem
    public:
        SafeQueue() {}
        ~SafeQueue() {}
        bool empty() const { std::shared_lock<std::shared_mutex> lock {_mtx}; return _que.empty(); }
        auto size() const { std::shared_lock<std::shared_mutex> lock {_mtx}; return _que.size(); }
        template<typename U>
        void enqueue(U&& data) {
            static_assert(std::is_same_v<std::remove_reference_t<U>, T>);
            std::unique_lock<std::shared_mutex> lock {_mtx};
            _que.emplace(std::forward<U>(data));
        }
        std::optional<T> dequeue() { // in order to grammatically beautiful.
            std::unique_lock<std::shared_mutex> lock {_mtx};
            if (_que.empty()) return std::nullopt;
            std::optional<T> ret {std::move(_que.front())};
            _que.pop(); return ret;
        }
    };
}

#endif // __SAFE_QUEUE_HPP__