// standard
#include <bits/chrono.h>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <iterator>
#include <cstddef>
#include <chrono>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <queue>

// laar
#include <common/exceptions.hpp>
#include <common/callback.hpp>
#include <common/macros.hpp>

// local
#include "callback-queue.hpp"

using namespace laar;

CallbackQueue::CallbackQueue(CallbackQueueSettings settings, Private access)
: settings_(std::move(settings))
, abort_(false)
, initiated_(false)
{}

std::shared_ptr<CallbackQueue> CallbackQueue::configure(CallbackQueueSettings settings) {
    return std::make_shared<CallbackQueue>(std::move(settings), Private());
}

CallbackQueue::~CallbackQueue() {
    { 
        std::unique_lock<std::mutex> queueLock(queueLock_), schedulerLock(schedulerLock_); 
        abort_ = true;
    }
    queueCv_.notify_one();
    schedulerCv_.notify_one();

    schedulerThread_.join();
    workerThread_.join();
}

void CallbackQueue::init() {
    if (initiated_) {
        throw laar::LaarBadInit();
    }
    workerThread_ = std::thread{&CallbackQueue::run, this};
    schedulerThread_ = std::thread{&CallbackQueue::schedule, this};
    initiated_ = true;
}

void CallbackQueue::run() {
    genericCallback_t task;
    
    while (true) {
        {
            std::unique_lock<std::mutex> queueLock (queueLock_);
            queueCv_.wait(queueLock, [this] () {
                return !tasks_.empty() || abort_;
            });

            if (abort_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop();
        }
        if (isCastDownPossible<OptionalCallback>(task)) {
            handleOptionalCallback(specifyCallback<OptionalCallback>(std::move(task)));
        }
        if (isCastDownPossible<TimedCallback>(task)) {
            handleTimedCallback(specifyCallback<TimedCallback>(std::move(task)));
        }
        if (isCastDownPossible<Callback>(task)) {
            handleRegularCallback(specifyCallback<Callback>(std::move(task)));
        }
    }
}

void CallbackQueue::schedule() {
    timedCallback_t next;
    while (true) {
        {
            std::unique_lock<std::mutex> schedulerLock (schedulerLock_);
            schedulerCv_.wait(schedulerLock, [this]() {
                return !scheduled_.empty() || abort_;
            });

            if (abort_) {
                return;
            }

            next = std::move(const_cast<timedCallback_t&>(scheduled_.top()));
            scheduled_.pop();
        }
        std::this_thread::sleep_for(next->goesOff() - std::chrono::high_resolution_clock::now());
        {
            std::unique_lock<std::mutex> queueLock (queueLock_);
            tasks_.push(std::move(next));
        }
        queueCv_.notify_one();
    }
}

void CallbackQueue::handleRegularCallback(regularCallback_t callback) {
    if (callback->validate()) callback->execute();
}

void CallbackQueue::handleOptionalCallback(optionalCallback_t callback) {
    if (callback->validate()) callback->execute();
}

void CallbackQueue::handleTimedCallback(timedCallback_t callback) {
    if (!callback->validate() && !callback->locked()) {
        {
            std::unique_lock<std::mutex> schedulerLock (schedulerLock_);
            if (scheduled_.size() >= settings_.maxScheduled) {
                return;
            }
            scheduled_.emplace(std::move(callback));
        }
        schedulerCv_.notify_one();
    } else if (callback->validate()) {
        callback->execute();
    }
}

void CallbackQueue::query(std::function<void()> task) {
    query(makeCallback<Callback>(std::move(task)));
}

void CallbackQueue::query(std::function<void()> task, std::chrono::milliseconds timeout) {
    query(makeCallback<TimedCallback>(std::move(task), timeout));
}

void CallbackQueue::query(std::function<void()> task, std::weak_ptr<void> lifetime) {
    query(makeCallback<OptionalCallback>(std::move(task), lifetime));
}

void CallbackQueue::query(genericCallback_t callback) {
    {
        std::unique_lock<std::mutex> lock;
        if (tasks_.size() >= settings_.maxSize) return;
        tasks_.emplace(std::move(callback));
    }
    queueCv_.notify_one();
}