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
        using optionalCallback_t = std::unique_ptr<OptionalCallback>;
        using regularCallback_t = std::unique_ptr<Callback>;
        using genericCallback_t = std::unique_ptr<ICallback>;

        struct Settings {
            std::size_t size = std::thread::hardware_concurrency();
            std::size_t maxSize = 1024;
        };

        static std::shared_ptr<ThreadPool> configure(Settings settings);
        ThreadPool(Settings settings, Private access);
        ~ThreadPool();

        void init();
        void query(std::function<void()> task, std::weak_ptr<void> lifetime);
        void query(std::function<void()> task);

    private:
        void run(); 
        void query(genericCallback_t callback);
        void handleOptionalCallback(optionalCallback_t callback);
        void handleRegularCallback(regularCallback_t callback);

    private:
        bool abort_;
        bool initiated_;
        Settings settings_;

        std::vector<std::thread> threads_;
        std::queue<genericCallback_t> tasks_;
        std::condition_variable cv_;
        std::mutex queueMutex_;
    };

} // namespace laar