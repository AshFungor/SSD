// GTest
#include "common/callback-queue.hpp"
#include "util/config-loader.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

// plog
#include <ios>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Severity.h>

// standard
#include <iostream>
#include <atomic>
#include <memory>
#include <chrono>
#include <sstream>
#include <thread>
#include <vector>

// sockpp
#include <sockpp/tcp_connector.h>

// laar
#include <network/message.hpp>
#include <network/server.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>

using namespace std::chrono;

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

class ServerTest : public testing::Test {
public:

    void initConfigs() {
        nlohmann::json cfgDefault;

        cfgDefault["server"] = {};
        cfgDefault["server"]["hostname"] = "localhost";
        cfgDefault["server"]["port"] = 5050;
        cfgDefault["server"]["timeouts"] = {1, 1};

        std::ofstream osDefault {"default.cfg", std::ios::out | std::ios::binary};
        osDefault << cfgDefault;

        std::ofstream osDynamic {"dynamic.cfg"};
        osDynamic << "{}";
    }

    void SetUp() override {
        plog::init(plog::Severity::debug, "server.log", 1024 * 8, 2);
        PLOG(plog::debug) << "running server on local endpoint";

        initConfigs();

        SharedCallbackQueue = laar::CallbackQueue::configure({});
        PLOG(plog::debug) << "module created: " << "SharedCallbackQueue; instance: " << SharedCallbackQueue.get();
        configHandler = laar::ConfigHandler::configure("", SharedCallbackQueue);
        PLOG(plog::debug) << "module created: " << "ConfigHandler; instance: " << configHandler.get();

        SharedCallbackQueue->init();
        configHandler->init();

        threadPool = laar::ThreadPool::configure({});
        threadPool->init();
        
        PLOG(plog::debug) << "module created: " << "ThreadPool; instance: " << threadPool.get();

        server = srv::Server::configure(SharedCallbackQueue, threadPool, configHandler);
        PLOG(plog::debug) << "module created: " << "Server; instance: " << server.get();

        server->init();
    }

    void TearDown() override {

    }

public:
    std::shared_ptr<laar::ConfigHandler> configHandler;
    std::shared_ptr<laar::CallbackQueue> SharedCallbackQueue;
    std::shared_ptr<laar::ThreadPool> threadPool;
    std::shared_ptr<srv::Server> server;

};

namespace {

    std::unique_ptr<char[]> assembleMessage(
        laar::MessageBuilder::IResult::EVersion version,
        laar::MessageBuilder::IResult::EPayloadType payloadType,
        laar::MessageBuilder::IResult::EType type,
        char* buffer,
        std::size_t size) 
    {
        auto message = std::make_unique<char[]>(6 + size);
        std::size_t section;

        section = (std::uint32_t) version;
        section |= ((std::uint32_t) payloadType) << 4;
        section |= (((std::uint32_t) type)) << 8;

        section |= ((std::size_t) size) << 16;

        std::memcpy(message.get(), &section, 6);
        std::memcpy(message.get() + 6, buffer, size);

        return std::move(message);
    }

}

TEST_F(ServerTest, RunServerTests) {
    std::thread main (&srv::Server::run, server.get());
    std::this_thread::sleep_for(100ms);

    std::filesystem::remove("default.cfg");
    std::filesystem::remove("dynamic.cfg");
    server->terminate();
    main.join();
}

TEST_F(ServerTest, InitConnection) {
    std::thread main (&srv::Server::run, server.get());
    std::this_thread::sleep_for(100ms);

    sockpp::tcp_connector conn;
    int16_t port = 5050;

    if (auto res = conn.connect(sockpp::inet_address("localhost", port))) {
        GTEST_COUT("result of making connection: " << res.error_message());
    }

    NSound::TClientMessage message;
    *message.mutable_simple_message()->mutable_stream_config() = NSound::NSimple::TSimpleMessage::TStreamConfiguration::default_instance();
    message.mutable_simple_message()->mutable_stream_config()->mutable_buffer_config()->set_fragment_size(12);
    std::size_t len = message.ByteSizeLong();

    GTEST_COUT("sending message with " << len << " bytes");

    auto buffer = std::make_unique<char[]>(len);
    message.SerializeToArray(buffer.get(), len);

    auto out = assembleMessage(
        laar::MessageBuilder::IResult::EVersion::FIRST, 
        laar::MessageBuilder::IResult::EPayloadType::RAW, 
        laar::MessageBuilder::IResult::EType::CLOSE_SIMPLE, 
        buffer.get(), 
        len);

    conn.write_n(out.get(), len + 6);

    conn.close();

    std::this_thread::sleep_for(100ms);
    std::filesystem::remove("default.cfg");
    std::filesystem::remove("dynamic.cfg");
    server->terminate();
    main.join();
}



