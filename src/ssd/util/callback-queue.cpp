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

// local
#include "callback-queue.hpp"

using namespace util;

CallbackQueue::CallbackQueue(std::size_t maxSize)
: maxSize_(maxSize)
, abort_(false)
{}

#include <iostream>
CallbackQueue::~CallbackQueue() {
    { 
        std::unique_lock<std::mutex> lock(queueLock_); 
        abort_ = true; 
    }
    cv_.notify_one();
    underlyingProcess_.join();
}

void CallbackQueue::init() {
    underlyingProcess_ = std::thread{&CallbackQueue::run, this};
}

void CallbackQueue::sync() {
    std::unique_lock<std::mutex> lock (queueLock_);

    cv_.wait(lock, [this]() {
        return tasks_.empty();
    });
}

void CallbackQueue::run() {
    std::function<void()> currentTask;
    while (true) {
        {
            std::unique_lock<std::mutex> lock (queueLock_);
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
        cv_.notify_one();
    }
}

void CallbackQueue::query(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock;
        if (tasks_.size() >= maxSize_) return;
        tasks_.emplace(std::move(task));
    }
    cv_.notify_one();
}