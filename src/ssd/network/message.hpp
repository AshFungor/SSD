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
#include <plog/Severity.h>
#include <protos/client/client-message.pb.h>
#include <protos/client/simple/simple.pb.h>

// plog
#include <plog/Log.h>

namespace laar {

    // Size must be equal requestedBytes() byte count
    struct Receive {
        Receive(std::size_t size);
        std::size_t size;
    };

    template<typename Payload>
    class SimpleMessage : public IMessage<SimpleMessage<Payload>> {
    public:

        // IMessage implementation
        virtual void reset() override;
        virtual void start() override;

        SimpleMessage(std::shared_ptr<laar::SharedBuffer> buffer);

        struct MessageData {
            std::size_t size;
            Payload payload;
        };

        bool constructed();
        std::unique_ptr<MessageData> result();

        class IEventStateHandle : public IMessage<SimpleMessage<Payload>>::IMessageState {
        public:
            IEventStateHandle(SimpleMessage<Payload>* data) : IMessage<SimpleMessage<Payload>>::IMessageState(data) {}
            virtual void event(Receive event) {}
        };

        class Size : IEventStateHandle {
        public:
            Size(SimpleMessage<Payload>* data)          : IEventStateHandle(data) {}
            // IMessageState implementation
            virtual void entry() override               { bytes_ = sizeof this->fsm_->data_->size; }
            virtual void exit() override                {}
            virtual std::size_t bytes() const override  { return bytes_; }

            void event(Receive event) override;

            friend SimpleMessage<Payload>;

        private:
            std::size_t bytes_;
        };

        class Data : IEventStateHandle {
        public:
            Data(SimpleMessage<Payload>* data)          : IEventStateHandle(data) {}
            // IMessageState implementation
            virtual void entry() override               { bytes_ = this->fsm_->data_->size; }
            virtual void exit() override                {}
            virtual std::size_t bytes() const override  { return bytes_; }

            virtual void event(Receive event) override;

            friend SimpleMessage<Payload>;

        private:
            std::size_t bytes_;
        };

        class Trail : IEventStateHandle {
        public:
            Trail(SimpleMessage<Payload>* data)         : IEventStateHandle(data) {}
            // IMessageState implementation
            virtual void entry() override               { bytes_ = 0; }
            virtual void exit() override                {}
            virtual std::size_t bytes() const override  { return bytes_; }

            virtual void event(Receive event) override;

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
        this->data_ = std::make_unique<SimpleMessage<Payload>::MessageData>();
        this->current_ = &sizeState_;
        this->current_->entry();
    }

    template<typename Payload>
    void SimpleMessage<Payload>::start() {
        reset();
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
    std::unique_ptr<typename SimpleMessage<Payload>::MessageData> SimpleMessage<Payload>::result() {
        if (constructed()) {
            return std::move(this->data_);
        }
        throw laar::LaarBadGet();
    }

    template<typename Payload>
    void SimpleMessage<Payload>::Size::event(Receive event) {
        if (event.size > bytes_) {
            throw laar::LaarOverrun(event.size - bytes_);
        }

        bytes_ -= event.size;
        if (!bytes_) {
            std::memcpy(
                &this->fsm_->data_->size, 
                this->fsm_->buffer_->getRawReadCursor(), 
                sizeof this->fsm_->data_->size
            );
            this->fsm_->buffer_->advanceReadCursor(sizeof this->fsm_->data_->size);
            PLOG(plog::debug) << "received size: " << this->fsm_->data_->size;
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
            auto res = this->fsm_->data_->payload.ParseFromArray(this->fsm_->buffer_->getRawReadCursor(), this->fsm_->data_->size);
            this->fsm_->buffer_->advanceReadCursor(this->fsm_->data_->size);
            PLOG(plog::debug) << "received proto: " << res;
            this->transition(&this->fsm_->dataState_);
        }
    }

    template<typename Payload>
    void SimpleMessage<Payload>::Trail::event(Receive event) {
        throw laar::LaarOverrun();
    }
}
