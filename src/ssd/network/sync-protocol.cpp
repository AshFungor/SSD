// standard
#include <initializer_list>
#include <cstddef>
#include <memory>
#include <queue>

// laar
#include <network/interfaces/i-protocol.hpp>
#include <common/plain-buffer.hpp>
#include <common/exceptions.hpp>
#include <network/interfaces/i-message.hpp>
#include <protos/client/simple/simple.pb.h>
#include <sounds/write-handle.hpp>
#include <sounds/read-handle.hpp>

// proto
#include <protos/client/client-message.pb.h>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// local
#include "sync-protocol.hpp"

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

    std::string dumpStreamConfig(const NSound::NSimple::TSimpleMessage::TStreamConfiguration& message) {
        std::stringstream ss;
        ss << "Message parsed: NSound::NSimple::TSimpleMessage::TStreamConfiguration\n";
        if (message.has_buffer_config()) {
            const auto& msg = message.buffer_config();
            ss << "- TBufferConfig: present\n";
            ss << "  size: " << msg.size() << "\n";
            ss << "  prebuffing_size: " << msg.prebuffing_size() << "\n";
            ss << "  min_request_size: " << msg.min_request_size() << "\n";
            ss << "  fragment_size: " << msg.fragment_size() << "\n";
        } else {
            ss << "- TBufferConfig: missing\n";
        }
        if (message.has_channel_map()) {
            ss << "- TChannelMap: present\n";
            for (const auto& channel : message.channel_map().mapped_channel()) {
                ss << "  Channel value:\n";
                if (channel.has_position()) {
                    ss << "  Position: " << channel.position() << "\n";
                }
                if (channel.has_aux()) {
                    ss << "  AUX: " << channel.aux() << "\n"; 
                }
            }
        } else {
            ss << "- TChannelMap: missing\n";
        }
        if (message.has_client_name()) {
            ss << "- ClientName: " << message.client_name() << "\n";
        } else {
            ss << "- ClientName: missing\n"; 
        }
        if (message.has_stream_name()) {
            ss << "- StreamName: " << message.has_stream_name() << "\n";
        } else {
            ss << "- StreamName: missing\n"; 
        }
        if (message.has_sample_spec()) {
            const auto& msg = message.sample_spec();
            ss << "- TSampleSpec: present\n";
            ss << "  Format: " << msg.format() << "\n";
            ss << "  SampleRate: " << msg.sample_rate() << "\n";
            ss << "  Channels: " << msg.channels() << "\n";
        } else {
            ss << "- TSampleSpec: missing\n";
        }
        ss << "- Direction: " << message.direction() << "\n";
        ss << "Parsing ended.";
        return ss.str();
    }

    void fillConfig(NSound::NSimple::TSimpleMessage::TStreamConfiguration& config) {

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

std::size_t MessageQueue::push(std::unique_ptr<IResult> message) {
    auto effective = classifier_->classify(message);
    effectiveSize_ += effective;
    msQueue_.emplace(std::move(message));
    return effective;
}

std::size_t MessageQueue::getEffectiveLoad() const {
    return effectiveSize_;
}

std::shared_ptr<SyncProtocol> SyncProtocol::configure(
    std::weak_ptr<IProtocol::IReplyListener> listener,
    std::shared_ptr<laar::IStreamHandler> soundHandler) 
{
    return std::make_shared<SyncProtocol>(std::move(listener), std::move(soundHandler), Private{});
}

SyncProtocol::SyncProtocol(
    std::weak_ptr<IProtocol::IReplyListener> listener,
    std::shared_ptr<laar::IStreamHandler> soundHandler,
    Private access) 
: listener_(std::move(listener))
, soundHandler_(std::move(soundHandler))
{}

void SyncProtocol::onClientMessage(std::unique_ptr<IResult> message) {
    auto payload = message->cast<IStructuredResult>().payload().mutable_simple_message();
    if (config_.has_value()) {
        switch (message->type()) {
            case IResult::EType::OPEN_SIMPLE:
                onError(laar::LaarSemanticError("stream is already opened, though OPN_SIMPLE received"));
                break;
            case IResult::EType::CLOSE_SIMPLE:
                onClose(std::move(payload->close()));
                break;
            case IResult::EType::DRAIN_SIMPLE:
                onDrain(std::move(payload->drain()));
                break;
            case IResult::EType::FLUSH_SIMPLE:
                onFlush(std::move(payload->flush()));
                break;
            case IResult::EType::PUSH_SIMPLE:
                onIOOperation(std::move(payload->push()));
                break;
            case IResult::EType::PULL_SIMPLE:
                onIOOperation(std::move(payload->pull()));
                break;
        }
    } else {
        if (message->type() == IResult::EType::OPEN_SIMPLE) {
            onStreamConfiguration(payload->stream_config());
        } else {
            onError(laar::LaarSemanticError("expecting stream configuration as first message"));
        }
    }
}

void SyncProtocol::onStreamConfiguration(NSound::NSimple::TSimpleMessage::TStreamConfiguration message) {
    PLOG(plog::info) << "received config for sync stream on protocol " << this << ": " << dumpStreamConfig(message);
    fillConfig(message);
    config_ = std::move(message);

    if (auto audio = soundHandler_.lock()) {
        if (config_->direction() == TSimpleMessage::TStreamConfiguration::PLAYBACK) {
            handle_ = audio->acquireWriteHandle(config_.value(), weak_from_this());
        } else if (config_->direction() == TSimpleMessage::TStreamConfiguration::RECORD) {
            handle_ = audio->acquireReadHandle(config_.value(), weak_from_this());
        }
    } else {
        PLOG(plog::warning) << "sound handler is nullptr, session was not started";
    }
}

void SyncProtocol::onDrain(NSound::NSimple::TSimpleMessage::TDrain message) {
    PLOG(plog::debug) << "received drain directive on protocol " << this;
}

void SyncProtocol::onFlush(NSound::NSimple::TSimpleMessage::TFlush message) {
    PLOG(plog::debug) << "received flush directive on protocol " << this;
}

void SyncProtocol::onIOOperation(NSound::NSimple::TSimpleMessage::TPull message) {
    PLOG(plog::debug) << "received IO operation (pull) directive on protocol " << this << " effective size is " << message.size();
    if (config_->direction() == TSimpleMessage::TStreamConfiguration::PLAYBACK) {
        onError(laar::LaarSemanticError("expecting push directives on playback; received pull"));
    } else if (config_->direction() == TSimpleMessage::TStreamConfiguration::RECORD) {
        // not implemented
    }
}

void SyncProtocol::onIOOperation(NSound::NSimple::TSimpleMessage::TPush message) {
    PLOG(plog::debug) << "received IO operation (push) directive on protocol " << this << " effective size is " << message.size();
    if (config_->direction() == TSimpleMessage::TStreamConfiguration::PLAYBACK) {
        auto status = handle_->write(message.pushed().c_str(), message.size());
        PLOG(plog::debug) << "write status: " << status;
    } else if (config_->direction() == TSimpleMessage::TStreamConfiguration::RECORD) {
        onError(laar::LaarSemanticError("expecting pull directives on record; received push"));
    }
}

void SyncProtocol::onClose(NSound::NSimple::TSimpleMessage::TClose message) {
    PLOG(plog::info) << "received close on protocol " << this;
}

void SyncProtocol::onError(std::exception error) {
    PLOG(plog::error) << "error on protocol " << this << ": " << error.what();
    throw error; 
}

void SyncProtocol::onBufferDrained(int status) {

}

void SyncProtocol::onBufferFlushed(int status) {

}

