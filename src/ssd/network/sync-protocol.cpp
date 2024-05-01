// standard
#include <cstddef>
#include <initializer_list>
#include <memory>

// laar
#include <network/interfaces/i-protocol.hpp>
#include <common/plain-buffer.hpp>
#include <common/exceptions.hpp>

// proto
#include <protos/client/client-message.pb.h>
#include <queue>

// local
#include "sync-protocol.hpp"
#include "network/interfaces/i-message.hpp"

using namespace laar;

namespace {

    bool typeIn(IResult::EType type, std::initializer_list<IResult::EType> matches) {
        bool in = false;
        for (const auto& match : matches) {
            if (type == match) {
                return true;
            }
        }
        return false;
    }

}

MessageQueue::MessageQueue(std::unique_ptr<IEffectiveLoadClassifier> classifier) 
: classifier_(std::move(classifier))
, effectiveSize_(0)
{}

std::size_t SimpleEffectiveLoadClassifier::classify(const std::unique_ptr<IResult>& message) {
    if (typeIn(message->type(), {IResult::EType::PULL_SIMPLE, IResult::EType::PUSH_SIMPLE})) {
        return message->size();
    }
    return 0;
}

std::unique_ptr<IResult> MessageQueue::pop() {
    auto out = std::move(msQueue_.front());
    effectiveSize_ -= classifier_->classify(out);
    msQueue_.pop();
    return out;
}

void MessageQueue::push(std::unique_ptr<IResult> message) {
    effectiveSize_ += classifier_->classify(message);
    msQueue_.emplace(message);
}

std::size_t MessageQueue::getEffectiveLoad() const {
    return effectiveSize_;
}

void SyncProtocol::onClientMessage(std::unique_ptr<IResult> message) {
    if (config_.has_value()) {
        switch (message->type()) {
            case IResult::EType::OPEN_SIMPLE:
                onError(laar::LaarSemanticError());
                break;
            case IResult::EType::CLOSE_SIMPLE:
                leave();
                break;
            case IResult::EType::DRAIN_SIMPLE:
                // handle drain
                break;
            case IResult::EType::FLUSH_SIMPLE:
                // handle flush
                break;
            case IResult::EType::PUSH_SIMPLE:
            case IResult::EType::PULL_SIMPLE:
                msQueue_->push(std::move(message));
                break;
        }
    }
}
