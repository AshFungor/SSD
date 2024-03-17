// GTest
#include <gtest/gtest.h>

// standard
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <vector>

// laar
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>

using namespace std::chrono;

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

class CallbackQueueTest : public testing::Test {
protected:
    std::shared_ptr<laar::CallbackQueue> cbQueue;
    std::vector<int> sharedResource;

    void SetUp() override {
        cbQueue = laar::CallbackQueue::configure({});
        cbQueue->init();
    }
};

TEST_F(CallbackQueueTest, RegularSyncExecution) {
    std::vector<int> nums = {1, 2, 3};
    for (const auto& num : nums) {
        cbQueue->query([this, num = num]() {
            sharedResource.push_back(num);
        });
    }
    cbQueue->query([this, nums = nums]() {
        auto resIter = sharedResource.begin();
        for (const auto& num : nums) {
            EXPECT_EQ(num, *(resIter++));
        }    
    });
}

TEST_F(CallbackQueueTest, TimedCallbackTest) {
    auto ts = high_resolution_clock::now();
    std::vector<milliseconds> durations = {10ms, 20ms, 100ms};
    for (const auto& duration : durations) {
        cbQueue->query([this, ts = ts, duration = duration]() {
            auto actualDuration = high_resolution_clock::now() - ts;
            GTEST_COUT("Callback executed after: " << duration_cast<milliseconds>(actualDuration) << ", requested: " << duration_cast<milliseconds>(duration))
            EXPECT_TRUE(actualDuration >= duration) << "callback executed earlier by " << duration - actualDuration << " ms!";
        }, duration);
    }
    auto hold = durations.back() * 2;
    GTEST_COUT("Waiting for " << duration_cast<milliseconds>(hold));
    std::this_thread::sleep_for(hold);
}

TEST_F(CallbackQueueTest, OptionalCallbackTest) {
    auto danglingResourse = std::make_shared<int>(1);
    cbQueue->query([&danglingResourse]() mutable {
        EXPECT_EQ(*danglingResourse, 1);
        danglingResourse = nullptr;
    }, danglingResourse);
    cbQueue->query([&danglingResourse]() {
        EXPECT_FALSE(true) << "This callback should be ignored since " << danglingResourse << " is null!";
    }, danglingResourse);
}

TEST_F(CallbackQueueTest, RestrictedDoubleInit) {
    EXPECT_THROW(cbQueue->init(), laar::LaarBadInit);
}