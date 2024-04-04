#pragma once

// standard
#include <cstddef>
#include <memory>

// laar
#include <common/shared-buffer.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>

// proto
#include <google/protobuf/message.h>
#include <protos/client/client-message.pb.h>
#include <protos/client/simple/simple.pb.h>

// msfsm
#include <msfsm.hpp>

namespace laar {

    struct IEvent {
        // holder for payload
    };

    // Size must be equal requestedBytes() byte count
    struct Receive : public IEvent {
        Receive(std::size_t size);
        std::size_t size;
    };

    class IMessage {
    public:
        class IMessageState : msfsm::noncopyable {
        public:
            IMessageState(IMessage* message);
            // generic state interface
            virtual void entry();
            virtual void exit();
            virtual void event(IEvent event);
            virtual std::size_t bytes() const;
            // transition function is always the same
            void transition(IMessageState& next);

        protected:
            IMessage* const fsm;
        };

        virtual void reset() = 0;
        virtual void start() = 0;
        void handle(IEvent event);
        const IMessageState* state() const;

        template<typename State>
        bool isInState();
    };

    template<typename Payload>
    class SimpleMessage : public IMessage {
    public:
        // concepts
        static_assert(std::derived_from<Payload, google::protobuf::Message> == true);

        SimpleMessage(std::shared_ptr<laar::SharedBuffer> buffer);

        // IMessage implementation
        void reset();
        void start();

        // getters
        Payload getPayload();
        std::size_t requestedBytes();
        std::size_t getSize();

        // checkers
        bool hasSize();
        bool hasPayload();
        bool isInMessageSizeState();
        bool isInMessagePayloadState();
        bool isInMessageTrailState();

    private:
        // message data
        std::size_t size_;
        Payload payload_;
        std::shared_ptr<laar::SharedBuffer> buffer_;

        class MessageSize : public IMessage<SimpleMessage<Payload>>::IMessageState {
        public:
            MessageSize(SimpleMessage<Payload>* message);
            void entry();
            // ISimpleMessageState implementation
            virtual void event(Receive event) override;
            virtual std::size_t requestedBytes() const override;

            friend SimpleMessage<Payload>;

        private:
            std::size_t needs_;

        } messageSize_{this};

        class MessagePayload : public IMessage<SimpleMessage<Payload>>::IMessageState {
        public:
            MessagePayload(SimpleMessage<Payload>* message);
            void entry();
            // ISimpleMessageState implementation
            virtual void event(Receive event) override;
            virtual std::size_t requestedBytes() const override;

            friend SimpleMessage<Payload>;
        
        private:
            std::size_t needs_;

        } messagePayload_{this};

        class MessageTrail : public IMessage<SimpleMessage<Payload>>::IMessageState {
        public:
            MessageTrail(SimpleMessage<Payload>* message);
            // ISimpleMessageState implementation
            virtual void event(Receive event) override;
            virtual std::size_t requestedBytes() const override;

            friend SimpleMessage<Payload>;

        } messageTrail_{this};
    };

    template<typename MessageProtobufPayload>
    SimpleMessage<MessageProtobufPayload>::MessageSize::MessageSize(SimpleMessage<MessageProtobufPayload>* message)
    : SimpleMessage<MessageProtobufPayload>::State(message)
    {}

    template<typename MessageProtobufPayload>
    SimpleMessage<MessageProtobufPayload>::MessagePayload::MessagePayload(SimpleMessage<MessageProtobufPayload>* message)
    : SimpleMessage<MessageProtobufPayload>::State(message)
    {}

    template<typename MessageProtobufPayload>
    SimpleMessage<MessageProtobufPayload>::MessageTrail::MessageTrail(SimpleMessage<MessageProtobufPayload>* message)
    : SimpleMessage<MessageProtobufPayload>::State(message)
    {}

    template<typename MessageProtobufPayload>
    void SimpleMessage<MessageProtobufPayload>::MessageSize::entry() {
        needs_ = sizeof(std::size_t);
    }

    template<typename MessageProtobufPayload>
    void SimpleMessage<MessageProtobufPayload>::MessagePayload::entry() {
        needs_ = size_;
    }

    template<typename MessageProtobufPayload>
    void SimpleMessage<MessageProtobufPayload>::MessageSize::event(Receive event) {
        needs_ = std::max<int>(static_cast<int>(needs_) - event.size, 0);
        if (!needs_) {
            std::memcpy(&size_, buffer_->getRawReadCursor(), sizeof(std::size_t));
            buffer_->advanceReadCursor(sizeof(std::size_t));
            transition(messagePayload_);
        }
    }

    template<typename MessageProtobufPayload>
    void SimpleMessage<MessageProtobufPayload>::MessagePayload::event(Receive event) {
        needs_ = std::max<int>(static_cast<int>(needs_) - event.size, 0);
        if (!needs_) {
            payload_.ParseFromArray(buffer_->getRawReadCursor(), size_);
            buffer_->advanceReadCursor(size_);
            transition(messageTrail_);
        }
    }

    template<typename MessageProtobufPayload>
    void SimpleMessage<MessageProtobufPayload>::MessageTrail::event(Receive event) {
        throw laar::LaarOverrun();
    }

    template<typename MessageProtobufPayload>
    std::size_t SimpleMessage<MessageProtobufPayload>::MessageSize::requestedBytes() const {
        return needs_;
    }

    template<typename MessageProtobufPayload>
    std::size_t SimpleMessage<MessageProtobufPayload>::MessagePayload::requestedBytes() const {
        return needs_;
    }

    template<typename MessageProtobufPayload>
    std::size_t SimpleMessage<MessageProtobufPayload>::MessageTrail::requestedBytes() const {
        return 0;
    }

    template<typename MessageProtobufPayload>
    void SimpleMessage<MessageProtobufPayload>::reset() {
        // payload proto shouldn't be cleared since it will be not
        // accessible anyways, shared buffer is managed externally 
        size_ = 0;
        transition(messageSize_);
    }

    template<typename MessageProtobufPayload>
    void SimpleMessage<MessageProtobufPayload>::start() {
        transition(messageSize_);
    }

    template<typename MessageProtobufPayload>
    SimpleMessage<MessageProtobufPayload>::SimpleMessage(std::shared_ptr<laar::SharedBuffer> buffer)
    : buffer_(buffer)
    , size_(0)
    {}

    template<typename MessageProtobufPayload>
    MessageProtobufPayload SimpleMessage<MessageProtobufPayload>::getPayload() { 
        if (hasPayload()) {
            return std::move(payload_);
        }
        throw laar::LaarBadGet();
    }

    template<typename MessageProtobufPayload>
    std::size_t SimpleMessage<MessageProtobufPayload>::getSize() { 
        if (hasSize()) {
            return fsm.size_;
        }
        throw laar::LaarBadGet();
    }

    template<typename MessageProtobufPayload>
    bool SimpleMessage<MessageProtobufPayload>::hasPayload() {
        return dynamic_cast<MessageTrail*>(state());
    }

    template<typename MessageProtobufPayload>
    bool SimpleMessage<MessageProtobufPayload>::hasSize() {
        return !dynamic_cast<MessageSize*>(state());
    }

    template<typename MessageProtobufPayload>
    std::size_t SimpleMessage<MessageProtobufPayload>::requestedBytes() { 
        return static_cast<ISimpleMessageState*>(state())->requestedBytes();
    }
}
