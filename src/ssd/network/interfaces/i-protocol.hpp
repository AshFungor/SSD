#pragma once

// standard
#include <cstddef>
#include <memory>

// laar
#include <common/ring-buffer.hpp>
#include <common/exceptions.hpp>

// proto
#include <protos/client/client-message.pb.h>

// local
#include "i-message.hpp"

namespace laar {

    class IEffectiveLoadClassifier {
    public:
        virtual std::size_t classify(const std::unique_ptr<IResult>& message) = 0;
    };

    class IMessageQueue {
    public:
        virtual std::unique_ptr<IResult> pop() = 0;
        virtual void push(std::unique_ptr<IResult> message) = 0;
        virtual std::size_t getEffectiveLoad() const = 0;

        virtual ~IMessageQueue() = default;
    };

    class IProtocol {
    public:

        class IReplyListener {
        public:
            virtual void onReply(std::unique_ptr<char[]> buffer, std::size_t size) = 0;
            virtual void leave() = 0;
        };

        virtual void onClientMessage(std::unique_ptr<IResult> message) = 0;

        virtual ~IProtocol() = default;
    };

}