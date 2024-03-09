// nlohmann 
#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>

// GTest
#include <gtest/gtest.h>

// standard
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>

// laar
#include <common/callback-queue.hpp>
#include <util/config-loader.hpp>

using namespace laar;

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

namespace {
    void write(std::string path, const nlohmann::json& config) {
        std::ofstream ofs {path, std::ios::out | std::ios::binary};
        GTEST_COUT("Writing json to file: " << path);

        std::string dumped = config.dump(4);
        GTEST_COUT("Json data: " << dumped);

        ofs.write(dumped.data(), dumped.size());
        ofs.close();
    }
}

class ConfigLoaderTest : public testing::Test {
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

        handler->init();
    }
};

TEST_F(ConfigLoaderTest, SubscribeAndListen) {
    auto lifetime = std::make_shared<int>(1);
    handler->subscribeOnDefaultConfig(
        "default", 
        [this](const nlohmann::json& config) {
            EXPECT_EQ(config.value<std::string>("default", "Not found!"), "default");
        }, 
        lifetime);
    handler->subscribeOnDynamicConfig(
        "dynamic", 
        [this](const nlohmann::json& config) {
            EXPECT_EQ(config.value<std::string>("dynamic", "Not found!"), "dynamic");
        }, 
        lifetime);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Add more tests for dynamic config in the future