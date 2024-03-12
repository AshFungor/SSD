#pragma once

// standard
#include <condition_variable>
#include <functional>
#include <cstddef>
#include <chrono>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <queue>

// laar
#include <common/callback.hpp>
#include <common/macros.hpp>


namespace laar {

    class CallbackQueue 
        : public std::enable_shared_from_this<CallbackQueue>
    {
    private: struct Private{ };
    public:
        using timedCallback_t = std::unique_ptr<TimedCallback>;
        using optionalCallback_t = std::unique_ptr<OptionalCallback>;
        using regularCallback_t = std::unique_ptr<Callback>;
        using genericCallback_t = std::unique_ptr<ICallback>;

        struct CallbackQueueSettings {
            std::size_t maxSize = 1024;
            std::size_t maxScheduled = 256;

            bool discardQueueOnAbort = false;
        };

        CallbackQueue(CallbackQueueSettings settings, Private access);
        ~CallbackQueue();

        static std::shared_ptr<CallbackQueue> configure(CallbackQueueSettings settings);
        void init();

        void query(std::function<void()> task);
        void query(std::function<void()> task, std::chrono::milliseconds timeout);
        void query(std::function<void()> task, std::weak_ptr<void> lifetime);

    private:
        void query(genericCallback_t callback);

        void handleRegularCallback(regularCallback_t callback);
        void handleOptionalCallback(optionalCallback_t callback);
        void handleTimedCallback(timedCallback_t callback);

        void schedule();
        void run();

    private:
        bool abort_;
        bool initiated_;

        CallbackQueueSettings settings_;

        std::mutex queueLock_;
        std::mutex schedulerLock_;
        std::condition_variable queueCv_;
        std::condition_variable schedulerCv_;

        std::thread schedulerThread_;
        std::thread workerThread_;

        std::queue<std::unique_ptr<ICallback>> tasks_;

        class Later {
        public:
            bool operator()(const timedCallback_t& lhs, const timedCallback_t& rhs) {
                return lhs->goesOff() > rhs->goesOff();
            }
        };

        std::priority_queue<timedCallback_t, std::vector<timedCallback_t>, Later> scheduled_;
    };

} // namespace laar