// nlohmann 
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

// GTest
#include <gtest/gtest.h>

// standard
#include <mutex>
#include <memory>
#include <string>
#include <fstream>
#include <iostream>
#include <condition_variable>

// laar
#include <src/common/callback-queue.hpp>
#include <src/ssd/util/config-loader.hpp>

using namespace laar;

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

namespace {
    void write(std::string path, const nlohmann::json& config) {
        std::ofstream ofs {path, std::ios::out | std::ios::binary};
        GTEST_COUT("writing json to file: " << path);

        std::string dumped = config.dump(4);
        GTEST_COUT("json data: " << dumped);

        ofs.write(dumped.data(), dumped.size());
        ofs.close();
    }
}

class CallbackQueueTest : public testing::Test {
protected:

    std::shared_ptr<CallbackQueue> cbQueue;
    std::string testingConfigDir = BINARY_DIR;
    std::shared_ptr<ConfigHandler> handler;

    void SetUp() override {
        cbQueue = laar::CallbackQueue::configure({});
        handler = laar::ConfigHandler::configure(testingConfigDir, cbQueue);

        GTEST_COUT("Writing testing configs...")
        write(testingConfigDir + "default.cfg", {
            {"default", {{"default", "default"}}}
        });
        write(testingConfigDir + "dynamic.cfg", {
            {"dynamic", {{"dynamic", "dynamic"}}}
        });

        cbQueue->init();
        handler->init();
    }
};

TEST_F(CallbackQueueTest, SubscribeAndListen) {
    auto lifetime = std::make_shared<int>(1);
    std::mutex defaultLock, dynamicLock;
    std::condition_variable cv;

    handler->subscribeOnDefaultConfig(
        "default", 
        [&](const nlohmann::json& config) {
            GTEST_COUT("config received: " << config);
            EXPECT_EQ(config.value<std::string>("default", "Not found!"), "default");
            cv.notify_one();
        }, 
        lifetime);
    handler->subscribeOnDynamicConfig(
        "dynamic", 
        [&](const nlohmann::json& config) {
            GTEST_COUT("config received: " << config);
            EXPECT_EQ(config.value<std::string>("dynamic", "Not found!"), "dynamic");
            cv.notify_one();
        }, 
        lifetime);

    std::unique_lock<std::mutex> lock_1(defaultLock), lock_2(dynamicLock);
    cv.wait(lock_1);
    cv.wait(lock_2);
}

TEST_F(CallbackQueueTest, ReadUpdateToDynamicConfig) {
    auto lifetime = std::make_shared<int>(1);
    std::mutex dynamicLock;
    std::condition_variable cv;

    int runNum = 0;

    handler->subscribeOnDynamicConfig(
        "dynamic", 
        [&](const nlohmann::json& config) {
            std::lock_guard<std::mutex> lock(dynamicLock);
            if (runNum == 0) {
                GTEST_COUT("initial config received: " << config);
                EXPECT_EQ(config.value<std::string>("dynamic", "Not found!"), "dynamic");
                cv.notify_one();
            } else if (runNum == 1) {
                GTEST_COUT("new config received: " << config);
                EXPECT_EQ(config.value<std::string>("dynamic", "Not found!"), "new_value!");
                cv.notify_one();
            }
            ++runNum;
        }, 
        lifetime);

    cbQueue->query([&]() {
        write(testingConfigDir + "dynamic.cfg", {
            {"dynamic", {{"dynamic", "new_value!"}}}
        });
    });

    std::unique_lock<std::mutex> lock_1(dynamicLock);
    cv.wait(lock_1);
    cv.wait(lock_1);
}