// STD
#include <array>
#include <cstdint>
#include <memory>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// laar
#include <common/thread-pool.hpp>
#include <sounds/dispatchers/bass-router-dispatcher.hpp>

// local
#include "async-dispatching-job.hpp"

using namespace laar;

namespace {

    constexpr int channelsOut = 2;
    constexpr int threads = 1;

}


AsyncDispatchingJob::AsyncDispatchingJob(
    std::shared_ptr<laar::ThreadPool> pool,
    std::shared_ptr<laar::BassRouterDispatcher> dispatcher, 
    std::size_t samples, 
    std::unique_ptr<std::int32_t[]> buffer
) 
    : dispatcher_(std::move(dispatcher))
    , inBuffer_(std::move(buffer))
    , pool_(std::move(pool))
    , outBuffer_(std::make_unique<std::int32_t[]>(samples * channelsOut))
    , check_(0)
    , samples_(samples)
{
    initialize(samples);
}

void AsyncDispatchingJob::initialize(std::size_t samples) {
    std::array<std::function<void()>, threads> jobs;
    for (std::size_t thread = 0; thread < threads; ++thread) {
        std::size_t from = samples / threads * thread;
        std::size_t to = from + samples / threads;
        auto runner = [this, samples, from, to]() {
            auto result = dispatcher_->dispatch(inBuffer_.get() + from, outBuffer_.get() + from * channelsOut, to - from);
            if (result.isError()) {
                PLOG(plog::error) << "error during bass dispatching: " << result.getError();
                std::abort();
            }

            ++check_;
        };

        jobs[thread] = std::move(runner);
    }

    for (auto& job : jobs) {
        pool_->query(std::move(job));
    }
}

std::unique_ptr<std::int32_t[]> AsyncDispatchingJob::result() {
    if (!ready()) {
        // we are not ready
        auto out = std::make_unique<std::int32_t[]>(samples_ * channelsOut);
        for (std::size_t channel = 0; channel < channelsOut; ++channel) {
            for (std::size_t sample = 0; sample < samples_; ++sample) {
                out[channel * samples_ + sample] = inBuffer_[sample];
            }
        }

        return out;
    }

    return std::move(outBuffer_);
}

bool AsyncDispatchingJob::ready() const noexcept {
    return check_ >= threads;
}

AsyncDispatchingJob::~AsyncDispatchingJob() {
    if (!ready()) {
        PLOG(plog::error) << "error: job is being destroyed unfinished!";
        PLOG(plog::debug) << std::flush;
        std::abort();
    }
}