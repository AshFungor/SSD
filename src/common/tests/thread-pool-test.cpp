// GTest
#include <gtest/gtest.h>

// standard
#include <condition_variable>
#include <iostream>
#include <memory>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

// laar
#include <common/thread-pool.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>

using namespace std::chrono;

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

class ThreadPoolTest : public testing::Test {
protected:
    std::shared_ptr<laar::ThreadPool> threadPool;
    std::vector<int> sharedResource;

    void SetUp() override {
        threadPool = laar::ThreadPool::configure({});
        threadPool->init();
    }
};

TEST_F(ThreadPoolTest, AsyncExecution) {
    int value = 0, count = 3;
    std::mutex valueLock;
    std::condition_variable cv;

    for (int i = 0; i < count; ++i) {
        threadPool->query([&, i = i]() mutable {
            std::unique_lock<std::mutex> llock(valueLock);
            GTEST_COUT("Entering async callback execution, value is " << value);
            ++value;

            if (i == count) cv.notify_one();
        });
    }

    std::unique_lock<std::mutex> llock(valueLock);
    cv.wait_for(llock, std::chrono::seconds(1));

    if (value != count) {
        EXPECT_FALSE(true) << "After async increment called " << count 
                           << " times, value is " << value << ", expected " << count;
    }
}