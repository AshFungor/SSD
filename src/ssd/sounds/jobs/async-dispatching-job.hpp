#pragma once

// STD
#include <cstdint>
#include <memory>

// laar
#include <common/thread-pool.hpp>
#include <sounds/dispatchers/bass-router-dispatcher.hpp>


namespace laar {

    class AsyncDispatchingJob {
    public:

        AsyncDispatchingJob(
            std::shared_ptr<laar::ThreadPool> pool,
            std::shared_ptr<laar::BassRouterDispatcher> dispatcher, 
            std::size_t samples, 
            std::unique_ptr<std::int32_t[]> buffer
        );

        ~AsyncDispatchingJob();

        std::unique_ptr<std::int32_t[]> result();
        bool ready() const noexcept;

    private:
        void initialize(std::size_t samples);

    private:

        std::shared_ptr<laar::ThreadPool> pool_;

        std::shared_ptr<laar::BassRouterDispatcher> dispatcher_;
        std::unique_ptr<std::int32_t[]> inBuffer_;
        std::unique_ptr<std::int32_t[]> outBuffer_;

        std::atomic<int> check_;
        std::size_t samples_;

    };

}