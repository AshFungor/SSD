// // nlohmann 
// #include <nlohmann/json_fwd.hpp>
// #include <nlohmann/json.hpp>

// // GTest
// #include <gtest/gtest.h>

// // standard
// #include <filesystem>
// #include <iostream>
// #include <fstream>
// #include <memory>
// #include <string>

// // testing target
// #define private public
// #include <util/config-loader.hpp>
// #undef private

// using namespace util;

// #define GTEST_COUT(chain) \
//     std::cerr << "[INFO      ] " << chain << '\n';

// class CallbackQueueTest : public testing::Test {
// protected:

//     std::shared_ptr<CallbackQueue> cbQueue;
//     std::string testingConfigDir = BINARY_DIR;
//     std::string pathToDefault = testingConfigDir + "default.cfg";
//     std::unique_ptr<ConfigHandler> handler;

//     void SetUp() override {
//         cbQueue = std::make_shared<CallbackQueue>(100);
//         cbQueue->init();
//         handler = std::make_unique<ConfigHandler>(testingConfigDir, true, cbQueue);

//         std::ofstream ofs {pathToDefault, std::ios::out | std::ios::binary};
//         GTEST_COUT("Writing json to file: " << pathToDefault);

//         nlohmann::json json {
//             {"string", "example"},
//             {"integer", 12}
//         };
//         std::string dumped = json.dump(4);
//         GTEST_COUT("Json data: " << dumped);

//         ofs.write(dumped.data(), dumped.size());
//         ofs.close();

//         handler->init();
//     }

//     void TearDown() override {
//         cbQueue->query([this]() {
//             std::filesystem::remove(pathToDefault);
//         });
//         cbQueue->sync();
//     }
// };

// TEST_F(CallbackQueueTest, ReadAndWriteTest) {
//     EXPECT_EQ(handler->get(cfg::BASE).value<std::string>("string", "No string found!"), "example");
//     EXPECT_EQ(handler->get(cfg::BASE).value<int>("integer", 0), 12);

//     handler->modify(cfg::BASE)["newValue"] = "hello!";
//     EXPECT_EQ((*handler)[cfg::BASE].value<std::string>("newValue", "No string found!"), "hello!");

//     handler->flush();
//     cbQueue->query([this]() {
//         EXPECT_EQ(handler->isSupported(cfg::DYNAMIC), false);
//         EXPECT_EQ(handler->isSupported(cfg::BASE), true);
//     });
//     cbQueue->sync();
// }

// Add more tests for dynamic config in the future