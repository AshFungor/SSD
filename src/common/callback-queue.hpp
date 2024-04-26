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

        // callback queue uses simple tasks with no arguments
        using IConditionalCallback = class IConditionalCallback<>;
        using Callback = class PersistentCallback<>;
        using TimedCallback = class TimedCallback<>;
        using BoundCallback = class BoundCallback<>;
        using ICallback = class ICallback<>;

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

        void hang();

        bool isWorkerThread();

    private:
        void query(std::unique_ptr<IConditionalCallback> callback);
        void query(std::unique_ptr<TimedCallback> callback);

        void handleCallback(std::unique_ptr<IConditionalCallback> callback);

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

        std::queue<std::unique_ptr<IConditionalCallback>> tasks_;

        class Later {
        public:
            bool operator()(const std::unique_ptr<TimedCallback>& lhs, const std::unique_ptr<TimedCallback>& rhs) {
                return lhs->expires() > rhs->expires();
            }
        };

        std::priority_queue<std::unique_ptr<TimedCallback>, std::vector< std::unique_ptr<TimedCallback>>, Later> scheduled_;
    };

} // namespace laar