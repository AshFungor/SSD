// GTest
#include "protos/client/client-message.pb.h"
#include "protos/client/simple/simple.pb.h"
#include <google/protobuf/message.h>
#include <gtest/gtest.h>

// plog
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Severity.h>

// standard
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <ios>

// sockpp
#include <sockpp/tcp_connector.h>

// laar
#include <network/message.hpp>
#include <network/server.hpp>
#include <util/config-loader.hpp>
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>

using namespace std::chrono;

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

static bool loggerInited_ = false;

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
        if (!loggerInited_) {
            plog::init(plog::Severity::debug, "server.log", 1024 * 8, 1);
            loggerInited_ = true;
        }
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

    static constexpr int headerSize = 6;

    std::unique_ptr<char[]> assembleStructuredMessage(
        laar::IResult::EVersion version,
        laar::IResult::EPayloadType payloadType,
        laar::IResult::EType type,
        google::protobuf::Message& message) 
    {
        std::size_t len = message.ByteSizeLong();

        auto buffer = std::make_unique<char[]>(len);
        message.SerializeToArray(buffer.get(), len);

        auto out = std::make_unique<char[]>(headerSize + len);
        std::size_t section = 0;

        section = (std::uint32_t) version;
        section |= ((std::uint32_t) payloadType) << 4;
        section |= (((std::uint32_t) type)) << 8;

        section |= ((std::size_t) len) << 16;

        std::memcpy(out.get(), &section, headerSize);
        std::memcpy(out.get() + headerSize, buffer.get(), len);

        std::stringstream ss;
        ss << std::hex;
        for (std::size_t i = 0, newline = 0; i < len + headerSize; ++i, ++newline) {
            if (newline % 4 == 0) {
                ss << "\n";
            }
            ss << "byte " << i << " is " << (std::uint32_t) out[i] << ";\t";
        }
        GTEST_COUT(ss.str());
        GTEST_COUT("assembling message with " << len << " bytes");

        return std::move(out);
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

    // open stream
    NSound::TClientMessage message;
    message.mutable_simple_message()->mutable_stream_config()->mutable_buffer_config()->set_fragment_size(12);

    auto out = assembleStructuredMessage(
        laar::IResult::EVersion::FIRST, 
        laar::IResult::EPayloadType::STRUCTURED, 
        laar::IResult::EType::OPEN_SIMPLE, 
        message);

    conn.write_n(out.get(), message.ByteSizeLong() + headerSize);

    // send some data
    NSound::TClientMessage another_message;
    another_message.mutable_simple_message()->mutable_push()->set_size(3);
    another_message.mutable_simple_message()->mutable_push()->set_pushed({1, 1, 1});

    out = assembleStructuredMessage(
        laar::IResult::EVersion::FIRST, 
        laar::IResult::EPayloadType::STRUCTURED, 
        laar::IResult::EType::PUSH_SIMPLE, 
        another_message);

    GTEST_COUT("sending push...");
    conn.write_n(out.get(), another_message.ByteSizeLong() + headerSize);
    GTEST_COUT("sending push...");
    conn.write_n(out.get(), another_message.ByteSizeLong() + headerSize);
    GTEST_COUT("sending push...");
    conn.write_n(out.get(), another_message.ByteSizeLong() + headerSize);

    NSound::TClientMessage closing_message;
    *closing_message.mutable_simple_message()->mutable_close() = NSound::NSimple::TSimpleMessage::TClose::default_instance();

    out = assembleStructuredMessage(
        laar::IResult::EVersion::FIRST, 
        laar::IResult::EPayloadType::STRUCTURED, 
        laar::IResult::EType::CLOSE_SIMPLE, 
        closing_message);

    GTEST_COUT("sending close...");
    conn.write_n(out.get(), closing_message.ByteSizeLong() + headerSize);
    
    conn.close();

    std::this_thread::sleep_for(100ms);
    std::filesystem::remove("default.cfg");
    std::filesystem::remove("dynamic.cfg");
    server->terminate();
    main.join();
}



