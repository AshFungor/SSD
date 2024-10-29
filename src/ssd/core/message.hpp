#pragma once

// STD
#include <queue>
#include <memory>
#include <cstdint>
#include <concepts>

// protos
#include <protos/server-message.pb.h>

namespace laar {

    namespace message {

        namespace version {
            inline constexpr std::uint8_t FIRST = 0x01;
        }

        namespace type {
            inline constexpr std::uint8_t SIMPLE = 0x01;
            inline constexpr std::uint8_t PROTOBUF = 0x02;
        }

    }

    using MessageSimplePayloadType = std::uint32_t;
    using MessageProtobufPayloadType = NSound::TServiceMessage;

    template<std::uint8_t PayloadType>
    using MessagePayloadTypeMatch = 
        typename std::conditional<PayloadType == message::type::SIMPLE, 
            MessageSimplePayloadType, 
            typename std::conditional<PayloadType == message::type::PROTOBUF, 
                MessageProtobufPayloadType, 
                void>::type
        >::type;

    // Protocol message provides API for message parsing and writing
    // from and to raw data streams.
    class Message {
    public:

        using SizeType = std::uint32_t;

        // Input
        void readFromArray(void* data, std::size_t maxBytes);
        void readHeaderFromArray(void* data, std::size_t size);
        void readVariableFromArray(void* data, std::size_t size);
        void readPayloadFromArray(void* data, std::size_t size);
        // Output
        void writeToArray(void* data, std::size_t maxBytes) const;
        void writeHeaderToArray(void* data, std::size_t size) const;
        void writeVariableToArray(void* data, std::size_t size) const;
        void writePayloadToArray(void* data, std::size_t size) const;

        class Size {
        public:

            inline static constexpr std::size_t header() {
                return sizeof(Message::header_);
            }

            inline static std::size_t variable(const Message* message) {
                switch (message->type()) {
                    case message::type::SIMPLE:
                        return 0;
                    case message::type::PROTOBUF:
                        return sizeof(SizeType);
                }
                return 0;
            }

            inline static std::size_t payload(const Message* message) {
                return message->size_;
            }

            inline static std::size_t total(const Message* message) {
                return Size::header() + Size::variable(message) + Size::payload(message);
            }

        };

        bool compareMetadata(const Message& other) const;

        // attributes
        std::uint8_t version() const;
        std::uint8_t type() const;
        SizeType size() const;

        template<std::uint8_t PayloadType>
        friend auto messagePayload(Message* message) -> MessagePayloadTypeMatch<PayloadType>;

        friend class MessageFactory;

    private:
        // setters are for factory only
        void setVersion(std::uint8_t version);
        void setType(std::uint8_t type);
        void setSize(SizeType size);

        // state checking
        bool validVersion() const;
        bool validType() const;
        bool validSize() const;

    private:
        std::uint8_t header_ = 0;
        SizeType size_ = 0;
        // payload type depends on certain message
        std::variant<MessageProtobufPayloadType, MessageSimplePayloadType> payload_;

    };

    template<std::uint8_t PayloadType>
    auto messagePayload(Message* message) -> MessagePayloadTypeMatch<PayloadType> {
        if constexpr (PayloadType == message::type::SIMPLE) {
            return std::get<MessageSimplePayloadType>(message->payload_);
        } else if constexpr (PayloadType == message::type::PROTOBUF) {
            return std::get<MessageProtobufPayloadType>(std::move(message->payload_));
        }
    }

    template<std::uint8_t PayloadType>
    auto messagePayload(Message& message) -> MessagePayloadTypeMatch<PayloadType> {
        return messagePayload<PayloadType>(&message);
    }

    class MessageFactory 
        : std::enable_shared_from_this<MessageFactory> {
    public:

        static_assert(std::same_as<MessageSimplePayloadType, Message::SizeType>, 
            "factory applies optimizations based on the fact that simple payload type and message size type are equal");

        using MessageAddedSize = MessageSimplePayloadType;

        static std::shared_ptr<MessageFactory> configure();

        // parses next part of the message, returning size factory needs next.
        std::size_t parse(void* data, std::size_t& available);

        // acquire the oldest parsed message in queue.
        Message parsed();
        // acquire the oldest constructed message in queue.
        Message constructed();

        bool isConstructedAvailable() const;
        bool isParsedAvailable() const;

        // Message construction.
        MessageFactory& withMessageVersion(std::uint8_t version, bool persistent = true);
        MessageFactory& withType(std::uint8_t type, bool persistent = true);
        // method overrides to handle different payload types
        MessageFactory& withPayload(MessageProtobufPayloadType payload);
        MessageFactory& withPayload(MessageSimplePayloadType payload);
        // dump all available settings into message and move it to queue
        MessageFactory& construct();

        // returns current bytes wanted.
        std::size_t next() const;

    private:

        struct State {

            enum class EStage {
                HEADER, PAYLOAD
            } stage = EStage::HEADER;

            Message parsing;
            Message constructing;

        };

        MessageFactory() = default;
        MessageFactory(const MessageFactory&) = delete;
        MessageFactory(MessageFactory&&) = delete;

        void finishOnState(std::size_t& available, std::size_t shifted, State::EStage nextStage, bool renewMessage = false);

    private:
        std::queue<Message> parsed_;
        std::queue<Message> constructed_;

        // flags.
        std::uint8_t type_ = message::type::SIMPLE;
        std::uint8_t version_ = message::version::FIRST;

        // state for parsing & constructing.
        State state_;

    };

}