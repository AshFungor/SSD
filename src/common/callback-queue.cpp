// standard
#include <condition_variable>
#include <functional>
#include <chrono>
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
    if (!initiated_) return;
    { 
        std::unique_lock<std::mutex> queueLock(queueLock_), schedulerLock(schedulerLock_);
        if (settings_.discardQueueOnAbort) tasks_ = {};
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

bool CallbackQueue::isWorkerThread() {
    return std::this_thread::get_id() == workerThread_.get_id();
}

void CallbackQueue::hang() {
    std::condition_variable cv;
    std::mutex callingLock;
    std::unique_lock<std::mutex> locked(callingLock);

    bool isReached = false;

    query([&cv, &isReached]() {
        isReached = true;
        cv.notify_one();
    });

    cv.wait(locked, [&isReached]() {
        return !isReached;
    });
}

void CallbackQueue::run() {
    CallbackPtr task;
    TimedCallbackPtr timedTask;
    
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

            reschedule:
            next = std::move(const_cast<timedCallback_t&>(scheduled_.top()));
            scheduled_.pop();
            
            schedulerCv_.wait_for(schedulerLock, next->goesOff() - std::chrono::high_resolution_clock::now());
            if (std::chrono::high_resolution_clock::now() < next->goesOff()) {
                scheduled_.push(std::move(next));
                goto reschedule;
            }
        }

        
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
        std::unique_lock<std::mutex> lock (queueLock_);
        if (tasks_.size() >= settings_.maxSize) return;
        tasks_.emplace(std::move(callback));
    }
    queueCv_.notify_one();
}