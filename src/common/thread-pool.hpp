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
        : public std::enable_shared_from_this<ThreadPool>
    {
    private: struct Private { };
    public:

        using IConditionalCallback = class IConditionalCallback<>;
        using Callback = class PersistentCallback<>;
        using TimedCallback = class TimedCallback<>;
        using BoundCallback = class BoundCallback<>;
        using ICallback = class ICallback<>;

        struct Settings {
            std::size_t size = 2;
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
        void query(std::unique_ptr<IConditionalCallback> callback);
        void handleCallback(std::unique_ptr<IConditionalCallback> callback);

    private:
        bool abort_;
        bool initiated_;
        Settings settings_;

        std::vector<std::thread> threads_;
        std::queue<std::unique_ptr<IConditionalCallback>> tasks_;
        std::condition_variable cv_;
        std::mutex queueMutex_;
    };

} // namespace laar