// standard
#include <functional>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <thread>
#include <mutex>

// local
#include "thread-pool.hpp"

util::ThreadPool::ThreadPool(std::size_t threads) {
    threads = std::min<std::size_t>(threads, std::thread::hardware_concurrency());

    for (std::size_t i = 0; i < threads; ++i) {
        threads_.emplace_back([this] () {
            while (true) {
                std::function<void()> currentTask;
                {
                    std::unique_lock<std::mutex> lock (queue_mutex_);
                    cv_.wait(lock, [this] () {
                        return !tasks_.empty() || abort_;
                    });

                    if (abort_ && tasks_.empty()) {
                        return;
                    }

                    currentTask = std::move(tasks_.front());
                    tasks_.pop();
                }

                currentTask();
            }
        });
    }
}

util::ThreadPool::~ThreadPool() {
    { 
        std::unique_lock<std::mutex> lock(queue_mutex_); 
        abort_ = true; 
    }
    cv_.notify_all(); 
    for (auto& thread : threads_) { 
        thread.join(); 
    }
}

void util::ThreadPool::query(std::function<void()> task) {
    { 
        std::unique_lock<std::mutex> lock(queue_mutex_); 
        tasks_.emplace(std::move(task)); 
    } 
    cv_.notify_one();
}

util::ThreadPool& util::ThreadPool::getInstance() {
    if (instance_) {
        return *instance_;
    }
    instance_.reset(new ThreadPool(std::thread::hardware_concurrency()));
    return *instance_;
}