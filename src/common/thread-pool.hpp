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

// laar
#include <common/callback.hpp>
#include <common/exceptions.hpp>


namespace laar {

    class ThreadPool
    : std::enable_shared_from_this<ThreadPool>
    {
    private: struct Private { };
    public:
        struct Settings {
            std::size_t size = std::thread::hardware_concurrency();
        };

        static std::shared_ptr<ThreadPool> configure(Settings settings);
        ThreadPool(Settings settings, Private access);
        ~ThreadPool();

        void init();
        void query(std::function<void()> task, std::weak_ptr<void> lifetime);
        void query(std::function<void()> task);

    private:
        void query(ICallback callback);

    private:
        bool abort_;
        Settings settings_;

        std::vector<std::thread> threads_;
        std::queue<ICallback> tasks_;
        std::condition_variable cv_;
        std::mutex queue_mutex_;
    };

} // namespace laar