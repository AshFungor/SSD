#pragma once

// standard
#include <condition_variable>
#include <functional>
#include <iterator>
#include <cstddef>
#include <chrono>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <queue>

namespace util {

    class CallbackQueue {
    public:
        CallbackQueue(std::size_t maxSize);
        ~CallbackQueue();

        void init();
        void sync();
        void query(std::function<void()> task);
        /* may be implemented in the future 
        void queryWithTimeout(std::function<void()> task, std::chrono::milliseconds timeout);
        */
    private:
        void run();

    private:
        bool abort_;
        std::size_t maxSize_;
        std::mutex queueLock_;
        std::condition_variable cv_;
        std::thread underlyingProcess_;
        std::queue<std::function<void()>> tasks_;
    };

}