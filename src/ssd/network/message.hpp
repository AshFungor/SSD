#pragma once

// standard
#include <cstddef>
#include <initializer_list>
#include <memory>

// laar
#include <network/interfaces/i-message.hpp>
#include <common/shared-buffer.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>

// proto
#include <google/protobuf/message.h>
#include <protos/client/client-message.pb.h>
#include <protos/client/simple/simple.pb.h>

namespace laar {

    // Size must be equal requestedBytes() byte count
    struct Receive : public IEvent {
        Receive(std::size_t size);
        std::size_t size;
    };

    template<typename Payload>
    class SimpleMessage : public IMessage<SimpleMessage<Payload>> {
    public:

        // IMessage implementation
        virtual void reset() override;
        virtual void start() override;

        // supported events
        void handle(Receive event);

        SimpleMessage(std::shared_ptr<laar::SharedBuffer> buffer);

        struct MessageData {
            std::size_t size;
            Payload payload;
        };

        bool constructed();
        std::unique_ptr<MessageData> result();

        class Size : IMessage<SimpleMessage<Payload>>::IMessageState {
        public:
            Size(SimpleMessage<Payload>* data)          : IMessage<SimpleMessage<Payload>>::IMessageState(data) {}
            // IMessageState implementation
            virtual void entry() override               { bytes_ = sizeof this->fsm_->data_->size; }
            virtual void exit() override                {}
            virtual void event(IEvent event) override   { this->event(*static_cast<Receive*>(&event)); }
            virtual std::size_t bytes() const override  { return bytes_; }

            virtual void event(Receive event);

            friend SimpleMessage<Payload>;

        private:
            std::size_t bytes_;
        };

        class Data : IMessage<SimpleMessage<Payload>>::IMessageState {
        public:
            Data(SimpleMessage<Payload>* data)          : IMessage<SimpleMessage<Payload>>::IMessageState(data) {}
            // IMessageState implementation
            virtual void entry() override               { bytes_ = this->fsm_->data_->size; }
            virtual void exit() override                {}
            virtual void event(IEvent event) override   { this->event(*static_cast<Receive*>(&event)); }
            virtual std::size_t bytes() const override  { return bytes_; }

            virtual void event(Receive event);

            friend SimpleMessage<Payload>;

        private:
            std::size_t bytes_;
        };

        class Trail : IMessage<SimpleMessage<Payload>>::IMessageState {
        public:
            Trail(SimpleMessage<Payload>* data)         : IMessage<SimpleMessage<Payload>>::IMessageState(data) {}
            // IMessageState implementation
            virtual void entry() override               { bytes_ = 0; }
            virtual void exit() override                {}
            virtual void event(IEvent event) override   { this->event(*static_cast<Receive*>(&event)); }
            virtual std::size_t bytes() const override  { return bytes_; }

            virtual void event(Receive event);

            friend SimpleMessage<Payload>;

        private:
            std::size_t bytes_;

        };
        
    private:     
        std::unique_ptr<MessageData> data_;
        std::shared_ptr<laar::SharedBuffer> buffer_;

        Size sizeState_     {this};
        Data dataState_     {this};
        Trail trailState_   {this};
    };

    template<typename Payload>
    void SimpleMessage<Payload>::reset() {
        this->current_ = &sizeState_;
        this->current_->entry();
    }

    template<typename Payload>
    void SimpleMessage<Payload>::start() {
        this->current_->entry();
    }

    template<typename Payload>
    void SimpleMessage<Payload>::handle(Receive event) {
        this->current_->event(event);
    }

    template<typename Payload>
    SimpleMessage<Payload>::SimpleMessage(std::shared_ptr<laar::SharedBuffer> buffer)
    : IMessage<SimpleMessage<Payload>>({&sizeState_, &dataState_, &trailState_}, &sizeState_)
    , buffer_(buffer)
    {}

    template<typename Payload>
    bool SimpleMessage<Payload>::constructed() {
        return this->template isInState<Trail>();
    }

    template<typename Payload>
    void SimpleMessage<Payload>::Size::event(Receive event) {
        if (event.size > bytes_) {
            throw laar::LaarOverrun();
        }

        bytes_ -= event.size;
        if (!bytes_) {
            std::memcpy(
                &this->fsm_->data_->size, 
                this->fsm_->buffer_->getRawReadCursor(), 
                this->fsm_->data_->size
            );
            this->transition(&this->fsm_->dataState_);
        }
    }

    template<typename Payload>
    void SimpleMessage<Payload>::Data::event(Receive event) {
        if (event.size > bytes_) {
            throw laar::LaarOverrun();
        }

        bytes_ -= event.size;
        if (!bytes_) {
            this->fsm_->data_->payload.ParseFromArray(this->fsm_->buffer_->getRawReadCursor(), this->fsm_->data_->size);
            this->transition(&this->fsm_->dataState_);
        }
    }

    template<typename Payload>
    void SimpleMessage<Payload>::Trail::event(Receive event) {
        throw laar::LaarOverrun();
    }
}
