#pragma once

// standard
#include <concepts>
#include <cstdint>
#include <exception>
#include <cstddef>
#include <memory>

// laar
#include <common/ring-buffer.hpp>
#include <common/exceptions.hpp>

// proto
#include <protos/client/client-message.pb.h>

namespace laar {

    class IMessageReceiver;

    class IFallbackHandler {
    public:
        virtual void onError(IMessageReceiver* builder, std::exception error) = 0;
        virtual ~IFallbackHandler() = default;
    };

    class IResult {
    public:

        // command type
        enum class EType : std::uint8_t {
            OPEN_SIMPLE = 0,
            PULL_SIMPLE = 1,
            PUSH_SIMPLE = 2,
            CLOSE_SIMPLE = 3,
            DRAIN_SIMPLE = 4,
            FLUSH_SIMPLE = 5,
            // 250 more
        };

        // payload type
        enum class EPayloadType : std::uint8_t {
            STRUCTURED = 0,
            RAW = 1,
        };

        // protocol version
        enum class EVersion : std::uint8_t {
            FIRST,
            // 3 left
        };

        virtual EPayloadType payloadType() const = 0;
        virtual std::uint32_t size() const = 0;
        virtual EVersion version() const = 0;
        virtual EType type() const = 0;

        template<typename ResolvedResult>
        ResolvedResult& cast() requires std::derived_from<ResolvedResult, IResult> {
            return dynamic_cast<ResolvedResult&>(*this);
        }
    };

    class IRawResult : public IResult {
    public:
        virtual char* payload() = 0;
    };

    class IStructuredResult : public IResult {
    public:
        virtual NSound::TClientMessage& payload() = 0;
    };

    // Controls construction of messages
    class IMessageReceiver {
    public:

        IMessageReceiver(std::unique_ptr<IFallbackHandler> handle)
        : fbHandler_(std::move(handle))
        {}

        // Payload for state machine
        struct Receive {
            // Constructs payload
            Receive(std::size_t size)
            : size(size)
            {}
            
            // Received size
            std::size_t size;
        };        

        // resets construction process and puts initial state into ready
        virtual void reset() = 0;
        // inits builder, might be empty
        virtual void init() = 0;
        // check how many bytes of data needed for current chunk
        virtual std::uint32_t poll() const = 0;
        // handler for incoming data, returns true if construction is completed
        virtual bool handle(Receive payload) = 0;
        // checks if message is ready
        virtual operator bool() const = 0;
        // same as above
        virtual bool ready() const = 0;
        // gets resulting message, can be called only once per message
        virtual std::unique_ptr<IResult> fetch() = 0;
        // validates parser state
        virtual bool valid() const = 0;
        // invalidate current protocol transfer
        virtual void invalidate() = 0;

        virtual ~IMessageReceiver() = default;

    protected:
        virtual void fallback(std::exception error) {
            fbHandler_->onError(this, std::move(error));
        }

    protected:
        std::unique_ptr<IFallbackHandler> fbHandler_;
    };

    class IMessageSender {
    public:

        IMessageSender(std::unique_ptr<IFallbackHandler> handle)
        : fbHandler_(std::move(handle))
        {}

        // resets construction process and puts initial state into ready
        virtual void reset() = 0;
        // inits builder, might be empty
        virtual void init() = 0;
        // check how many bytes of data needed to transfer
        virtual std::uint32_t poll() const = 0;
        // move current state by number of sent bytes
        virtual void advance(std::uint32_t size) = 0;
        // query next message
        virtual void query(std::unique_ptr<IResult> next);
        // checks if message is ready
        virtual operator bool() const = 0;
        // same as above
        virtual bool ready() const = 0;
        // gets resulting message, can be called only once per message
        virtual std::unique_ptr<IResult> fetch() = 0;
        // validates parser state
        virtual bool valid() const = 0;
        // invalidate current protocol transfer
        virtual void invalidate() = 0;

    protected:
        std::unique_ptr<IFallbackHandler> fbHandler_;

    };
}