#pragma once

// standard
#include <cstddef>
#include <exception>
#include <memory>
#include <queue>

// laar
#include <network/interfaces/i-message.hpp>
#include <sounds/interfaces/i-audio-handler.hpp>
#include <network/interfaces/i-protocol.hpp>
#include <common/plain-buffer.hpp>
#include <common/exceptions.hpp>

// proto
#include <protos/client/client-message.pb.h>

namespace laar {

    class SimpleEffectiveLoadClassifier : public IEffectiveLoadClassifier {
    public:
        // IEffectiveLoadClassifier implementation
        virtual std::size_t classify(const std::unique_ptr<IResult>& message) override;
    };

    class MessageQueue: public IMessageQueue {
    public:

        MessageQueue(std::unique_ptr<IEffectiveLoadClassifier> classifier);
        // IMessageQueue implementation
        virtual std::unique_ptr<IResult> pop() override;
        virtual std::size_t push(std::unique_ptr<IResult> message) override;
        virtual std::size_t getEffectiveLoad() const override;

    private:
        std::unique_ptr<IEffectiveLoadClassifier> classifier_;
        std::queue<std::unique_ptr<IResult>> msQueue_;
        std::size_t effectiveSize_;
    };

    class SyncProtocol 
        : public IProtocol
        , public IStreamHandler::IHandle::IListener {
    public:

        SyncProtocol(std::weak_ptr<IProtocol::IReplyListener> listener);
        // IProtocol implementation
        virtual void onClientMessage(std::unique_ptr<IResult> message) override;
        // IStreamHandler::IHandle::IListener implementation
        virtual void onBufferDrained(int status) override;
        virtual void onBufferFlushed(int status) override;

    private:
        void onStreamConfiguration(NSound::NSimple::TSimpleMessage::TStreamConfiguration message);
        void onDrain(NSound::NSimple::TSimpleMessage::TDrain message);
        void onFlush(NSound::NSimple::TSimpleMessage::TFlush message);
        void onIOOperation(NSound::NSimple::TSimpleMessage::TPull message);
        void onIOOperation(NSound::NSimple::TSimpleMessage::TPush message);
        void onClose(NSound::NSimple::TSimpleMessage::TClose message);
        void onError(std::exception error);

    private:
        std::unique_ptr<MessageQueue> msQueue_;
        std::shared_ptr<laar::IStreamHandler::IHandle> handle_;
        std::optional<NSound::NSimple::TSimpleMessage::TStreamConfiguration> config_;
        std::weak_ptr<IProtocol::IReplyListener> listener_;
    };

}