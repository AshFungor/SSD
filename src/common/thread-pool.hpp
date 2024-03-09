#pragma once

// standard
#include <condition_variable>
#include <functional>
#include <cstddef>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <queue>

namespace util {

    template<typename Signature>
    std::function<void()> wrap(Signature target);

    class ThreadPool {
    public:
        static ThreadPool& getInstance();
        void query(std::function<void()> task);
        ~ThreadPool();

    private:
        ThreadPool() = delete;
        ThreadPool(const ThreadPool&) = delete;
        ThreadPool(const ThreadPool&&) = delete;
        // open thread pool API
        ThreadPool(std::size_t threads);

    private:
        // Singleton
        inline static std::unique_ptr<ThreadPool> instance_;  
        std::vector<std::thread> threads_;
        std::queue<std::function<void()>> tasks_;
        std::condition_variable cv_;
        std::mutex queue_mutex_;
        bool abort_;

    };

} // namespace util

template<typename Signature>
std::function<void()> util::wrap(Signature target) {
    std::function<void()> wrapped {[&target]() {
        target();
    }};
    return std::move(wrapped);
}