// standard
#include <functional>
#include <cstddef>
#include <memory>
#include <thread>
#include <mutex>

// local
#include "thread-pool.hpp"
#include "common/callback.hpp"

using namespace laar;


std::shared_ptr<ThreadPool> ThreadPool::configure(Settings settings) {
    return std::make_shared<ThreadPool>(std::move(settings), Private());
}

ThreadPool::ThreadPool(Settings settings, Private access)
: settings_(std::move(settings))
, initiated_(false)
, abort_(false)
{}

ThreadPool::~ThreadPool() {
    { 
        std::unique_lock<std::mutex> lock(queueMutex_); 
        abort_ = true; 
    }
    cv_.notify_all(); 
    for (auto& thread : threads_) { 
        thread.join(); 
    }
}

void ThreadPool::init() {
    if (initiated_) {
        throw laar::LaarBadInit();
    }
    for (std::size_t thread = 0; thread < settings_.size; ++thread) {
        threads_.emplace_back(std::thread(&ThreadPool::run, this));
    }

    initiated_ = true;
}

void ThreadPool::run() {
    std::unique_ptr<IConditionalCallback> currentTask;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            cv_.wait(lock, [this]() {
                return !tasks_.empty() || abort_;
            });

            if (abort_ && tasks_.empty()) {
                return;
            }

            currentTask = std::move(tasks_.front());
            tasks_.pop();
        }
        handleCallback(std::move(currentTask));
    }
}

void ThreadPool::query(std::function<void()> task, std::weak_ptr<void> lifetime) {
    query(ICallback::makeCallback<BoundCallback>(std::move(task), lifetime));
}

void ThreadPool::query(std::function<void()> task) {
    query(ICallback::makeCallback<Callback>(task));
}

void ThreadPool::query(std::unique_ptr<IConditionalCallback> callback) {
    {
        std::unique_lock<std::mutex> lock (queueMutex_);
        if (tasks_.size() >= settings_.maxSize) return;
        tasks_.emplace(std::move(callback));
    }
    cv_.notify_one();
}

void ThreadPool::handleCallback(std::unique_ptr<IConditionalCallback> callback) {
    if (callback->isValid()) callback->execute();
}