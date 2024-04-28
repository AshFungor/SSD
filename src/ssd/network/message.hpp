#pragma once

// standard
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <cstddef>
#include <memory>

// laar
#include <network/interfaces/i-message.hpp>
#include <common/ring-buffer.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>

// proto
#include <google/protobuf/message.h>
#include <protos/client/client-message.pb.h>
#include <protos/client/simple/simple.pb.h>

// plog
#include <plog/Log.h>

namespace laar {

    template<typename MessageBuilder>
    class RollbackHandler : IFallbackHandler<MessageBuilder> {
        virtual void onError(MessageBuilder* builder, std::exception error) override;
    };

    class MessageBuilder : IMessageBuilder<RollbackHandler<MessageBuilder>> {
    public:

        // need to patch this 
        class RawResult : public IRawResult {
        public:
            RawResult(
                std::weak_ptr<RingBuffer> buffer, 
                std::uint32_t size,
                EType type,
                EVersion version
            );
            ~RawResult();

            virtual std::size_t payload(char* dest) override;
            virtual std::uint32_t size() const override;
            virtual EVersion version() const override;
            virtual EType type() const override;

        private:
            bool hasPayload_;

            std::weak_ptr<RingBuffer> buffer_;
            std::uint32_t size_;
            EType type_;
            EVersion version_;
        };

        class StructuredResult : public IStructuredResult {
        public:
            StructuredResult(
                NSound::TClientMessage buffer, 
                std::uint32_t size,
                EType type,
                EVersion version
            );
        
            virtual NSound::TClientMessage& payload() override;
            virtual std::uint32_t size() const override;
            virtual EVersion version() const override;
            virtual EType type() const override;
        }; 

        MessageBuilder();
        // IMessageBuilder implementation
        virtual void reset() override;
        virtual void init() override;
        virtual std::uint32_t poll() const override;
        virtual bool handle(Receive payload) override;
        virtual operator bool() const override;
        virtual bool ready() const override;
        virtual std::unique_ptr<IResult> fetch() override;

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
