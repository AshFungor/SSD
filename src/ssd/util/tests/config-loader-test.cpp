// Boost
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/io_context.hpp>

// Abseil
#include <absl/strings/str_format.h>

// nlohmann 
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

// GTest
#include <gtest/gtest.h>

// Plog
#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Initializers/RollingFileInitializer.h>

// standard
#include <memory>
#include <string>
#include <fstream>
#include <iostream>

// laar
#include <src/ssd/util/config-loader.hpp>

using namespace laar;
using namespace std::chrono;

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

class ConfigHandlerTest : public testing::Test {
protected:

    std::shared_ptr<boost::asio::io_context> context;
    std::string testingConfigDir = BINARY_DIR;
    std::shared_ptr<ConfigHandler> handler;

    void SetUp() override {

        std::string logsFile = testingConfigDir + "test-log.txt";
        plog::init(plog::debug, logsFile.c_str());

        PLOG(plog::debug) << "initilizing logging for a test...";

        context = std::make_shared<boost::asio::io_context>();
        handler = laar::ConfigHandler::configure(testingConfigDir, context);

        GTEST_COUT("Writing testing configs...")
        write(testingConfigDir + "default.cfg", {
            {"default", {{"default", "default"}}}
        });
        write(testingConfigDir + "dynamic.cfg", {
            {"dynamic", {{"dynamic", "dynamic"}}}
        });
        
        if (auto status = handler->init(); !status.ok()) {
            GTEST_COUT(absl::StrFormat("Error: %s", status.message()))
            std::abort();
        }
    }
};

TEST_F(ConfigHandlerTest, SubscribeAndListen) {
    auto lifetime = std::make_shared<int>(1);
    std::atomic<int> counter = 2;

    handler->subscribeOnDefaultConfig(
        "default", 
        [&](const nlohmann::json& config) {
            GTEST_COUT("config received: " << config);
            EXPECT_EQ(config.value<std::string>("default", "Not found!"), "default");
            if (!--counter) {
                context->stop();
            }
        }, 
        lifetime
    );
    handler->subscribeOnDynamicConfig(
        "dynamic", 
        [&](const nlohmann::json& config) {
            GTEST_COUT("config received: " << config);
            EXPECT_EQ(config.value<std::string>("dynamic", "Not found!"), "dynamic");
            if (!--counter) {
                context->stop();
            }
        }, 
        lifetime
    );

    context->run();
}

TEST_F(ConfigHandlerTest, ReadUpdateToDynamicConfig) {
    auto lifetime = std::make_shared<int>(1);
    auto timer = std::make_unique<boost::asio::high_resolution_timer>(*context, 200ms);
    int runNum = 0;

    handler->subscribeOnDynamicConfig(
        "dynamic", 
        [&](const nlohmann::json& config) {
            if (runNum == 0) {
                GTEST_COUT("initial config received: " << config);
                EXPECT_EQ(config.value<std::string>("dynamic", "Not found!"), "dynamic");
                // put config update
                timer->async_wait([&](boost::system::error_code error) {
                    ASSERT_FALSE(error);
                    write(testingConfigDir + "dynamic.cfg", {
                        {"dynamic", {{"dynamic", "new_value!"}}}
                    });
                });
            } else if (runNum == 1) {
                GTEST_COUT("new config received: " << config);
                EXPECT_EQ(config.value<std::string>("dynamic", "Not found!"), "new_value!");
                // abort execution
                context->stop();
            }
            ++runNum;
        }, 
        lifetime);

    context->run();
}