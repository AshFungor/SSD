// GTest
#include "protos/server-message.pb.h"
#include "src/ssd/macros.hpp"
#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

// standard
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <type_traits>

// laar
#include <src/ssd/core/message.hpp>

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

namespace {

    void initMessage(NSound::TServiceMessage& message) {
        message.mutable_basemessage()->mutable_streamalteredconfiguration()->set_opened(true);
    }

    class MessageTest : public ::testing::Test {
    public:

        void SetUp() override {
            factory = laar::MessageFactory::configure();
        }

        std::shared_ptr<laar::MessageFactory> factory;

    };

}

TEST_F(MessageTest, TestConstruction) {
    factory
        ->withMessageVersion(laar::message::version::FIRST, true)
        .withType(laar::message::type::SIMPLE, true);

    // build some
    laar::MessageSimplePayloadType message = 0;
    const std::size_t messagesTotal = 20;

    for (std::size_t i = 0; i < messagesTotal; ++i) {
        factory->withPayload(message + i).construct();
    }

    std::size_t i = 0;
    while (factory->isConstructedAvailable()) {
        laar::Message popped = factory->constructed();

        // check for stuff
        ASSERT_EQ(popped.type(), laar::message::type::SIMPLE);
        ASSERT_EQ(popped.version(), laar::message::version::FIRST);

        laar::MessageSimplePayloadType payload = laar::messagePayload<laar::message::type::SIMPLE>(popped);
        ASSERT_EQ(payload, message + i);

        ++i;
    }

    ASSERT_EQ(i, messagesTotal);
}

TEST_F(MessageTest, TestProtobufConstruction) {
    factory
        ->withMessageVersion(laar::message::version::FIRST, true)
        .withType(laar::message::type::PROTOBUF, true);

    NSound::TServiceMessage message;
    initMessage(message);
    GTEST_COUT("writing message with size: " << message.ByteSizeLong())

    factory->withPayload(message).construct();

    ASSERT_EQ(factory->isConstructedAvailable(), true);
    laar::Message popped = factory->constructed();
    
    ASSERT_EQ(popped.type(), laar::message::type::PROTOBUF);
    ASSERT_EQ(popped.version(), laar::message::version::FIRST);
    laar::MessageProtobufPayloadType payload = laar::messagePayload<laar::message::type::PROTOBUF>(popped);
    ASSERT_EQ(google::protobuf::util::MessageDifferencer::Equals(payload, message), true);
}

TEST_F(MessageTest, TestParsing) {
    factory
        ->withMessageVersion(laar::message::version::FIRST, true)
        .withType(laar::message::type::SIMPLE, true)
        .withPayload(0)
        .construct();

    const std::size_t bufferSize = laar::MaxBytesOnMessage;
    auto buffer = std::make_unique<std::uint8_t[]>(bufferSize);

    laar::Message message = factory->constructed();
    message.writeToArray(buffer.get(), bufferSize);

    std::size_t remains = bufferSize;
    while (!factory->isParsedAvailable()) {
        factory->parse(buffer.get(), remains);
    }

    laar::Message parsed = factory->parsed();
    ASSERT_EQ(message.compareMetadata(parsed), true);
    ASSERT_EQ(laar::messagePayload<laar::message::type::SIMPLE>(message), 0);
}

TEST_F(MessageTest, TestParsingMultiple) {
    NSound::TServiceMessage message;
    initMessage(message);

    factory
        ->withMessageVersion(laar::message::version::FIRST, true)
        .withType(laar::message::type::PROTOBUF, true)
        .withPayload(message)
        .construct();

    const std::size_t bufferSize = laar::MaxBytesOnMessage;
    const std::size_t messagesTotal = 20;
    auto buffer = std::make_unique<std::uint8_t[]>(bufferSize);

    // get one message for comparison later
    factory->withPayload(message).construct();
    laar::Message standard = factory->constructed();

    std::size_t shift = 0;
    for (std::size_t i = 0; i < messagesTotal; ++i) {
        factory->withPayload(message).construct();
        laar::Message message = factory->constructed();
        message.writeToArray(buffer.get() + shift, bufferSize - shift);
        shift += message.byteSizeLong();
    }

    GTEST_COUT("written bytes to array: " << shift << "; per message: " << shift / messagesTotal)
    GTEST_COUT("header: " << laar::Message::Size::header() 
        << "; variable: " << laar::Message::Size::variable(&standard) 
        << "; payload: " << laar::Message::Size::payload(&standard))

    std::size_t remains = bufferSize;
    for (std::size_t i = 0; i < messagesTotal; ++i) {
        while (!factory->isParsedAvailable()) {
            factory->parse(buffer.get() + (bufferSize - remains), remains);
        }

        laar::Message parsed = factory->parsed();
        ASSERT_EQ(standard.compareMetadata(parsed), true);
        ASSERT_EQ(
            google::protobuf::util::MessageDifferencer::Equals(laar::messagePayload<laar::message::type::PROTOBUF>(standard), 
            message), 
        true);
    }

}