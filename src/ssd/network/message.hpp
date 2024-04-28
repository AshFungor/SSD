#pragma once

// standard
#include <cstdint>
#include <exception>
#include <cstddef>
#include <memory>

// laar
#include <network/interfaces/i-message.hpp>
#include <common/ring-buffer.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>

// proto
#include <google/protobuf/message.h>
#include <plog/Severity.h>
#include <protos/client/client-message.pb.h>
#include <protos/client/simple/simple.pb.h>

// plog
#include <plog/Log.h>

namespace laar {

    template<typename MessageBuilder>
    class RollbackHandler : IFallbackHandler<MessageBuilder> {
        virtual void onError(MessageBuilder* builder, std::exception error) override {
            PLOG(plog::warning) << "Protocol error: " << error.what();
            builder->stage_ = MessageBuilder::EState::INVALID;
        }
    };

    class MessageBuilder 
        : public IMessageBuilder<RollbackHandler<MessageBuilder>> 
        , public std::enable_shared_from_this<MessageBuilder> {
    private: struct Private {};
    public:

        class RawResult : public IRawResult {
        public:
            RawResult(
                std::shared_ptr<RingBuffer> buffer, 
                std::uint32_t size,
                EType type,
                EVersion version,
                EPayloadType payloadType
            );

            virtual char* payload() override;
            virtual std::uint32_t size() const override;
            virtual EVersion version() const override;
            virtual EType type() const override;
            virtual EPayloadType payloadType() const override;

        private:

            std::vector<char> buffer_;
            std::uint32_t size_;
            EType type_;
            EVersion version_;
            EPayloadType payloadType_;
        };

        class StructuredResult : public IStructuredResult {
        public:
            StructuredResult(
                NSound::TClientMessage message, 
                std::uint32_t size,
                EType type,
                EVersion version,
                EPayloadType payloadType
            );
        
            virtual NSound::TClientMessage& payload() override;
            virtual std::uint32_t size() const override;
            virtual EVersion version() const override;
            virtual EType type() const override;
            virtual EPayloadType payloadType() const override;

        private:

            NSound::TClientMessage message_;
            std::uint32_t size_;
            EType type_;
            EVersion version_;
            EPayloadType payloadType_;
        }; 

        static std::shared_ptr<MessageBuilder> configure(std::shared_ptr<laar::RingBuffer> buffer);
        MessageBuilder(std::shared_ptr<laar::RingBuffer> buffer, Private access);

        // IMessageBuilder implementation
        virtual void reset() override;
        virtual void init() override;
        virtual std::uint32_t poll() const override;
        virtual bool handle(Receive payload) override;
        virtual operator bool() const override;
        virtual bool ready() const override;
        virtual std::unique_ptr<IResult> fetch() override;
        virtual bool valid() const override;

        friend class RollbackHandler<MessageBuilder>;

    private:

        void onCriticalError(std::exception error);
        std::unique_ptr<IRawResult> assembleRaw();
        std::unique_ptr<IStructuredResult> assembleStructured();
        // parsing
        void handleHeader();
        void handlePayload();

    private:

        static constexpr std::size_t isRawMask = 0x10;
        static constexpr std::size_t versionMask = 0xF;
        static constexpr std::size_t messageTypeMask = 0xFF00;
        static constexpr std::size_t payloadSizeMask = 0xFFFFFFFF0000;

        enum class EState {
            IN_HEADER,
            IN_PAYLOAD,
            OUT_READY,
            INVALID,
        } stage_;

        // header size and structure is always defined and cannot
        // be changed.
        static constexpr std::uint32_t headerSize_ = 6;
        
        struct CachedMessageData {
            IResult::EVersion version;
            IResult::EType messageType;
            IResult::EPayloadType payloadType;
            std::uint32_t payloadSize;

        } current_;

        // bytes requested by current state
        std::uint32_t requested_;
        std::shared_ptr<laar::RingBuffer> buffer_;

        std::unique_ptr<IResult> assembled_;

    };
}
