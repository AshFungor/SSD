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
#include "protos/client/simple/simple.pb.h"

namespace laar {

    class IEffectiveLoadClassifier {
    public:
        virtual std::size_t classify(const std::unique_ptr<IResult>& message) = 0;
    };

    class IMessageQueue {
    public:
        virtual NSound::NSimple::TSimpleMessage pop() = 0;
        virtual std::size_t push(NSound::NSimple::TSimpleMessage&& message) = 0;
        virtual std::size_t load() const = 0;

        virtual ~IMessageQueue() = default;
    };

    class IProtocol {
    public:

        class IReplyListener {
        public:
            virtual void onReply(std::unique_ptr<char[]> buffer, std::size_t size) = 0;
            virtual void onClose() = 0;
        };

        virtual void onClientMessage(std::unique_ptr<IResult> message) = 0;

        virtual ~IProtocol() = default;
    };

}