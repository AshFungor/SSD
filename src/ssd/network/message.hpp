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

// plog
#include <plog/Log.h>

namespace laar {

    // Size must be equal requestedBytes() byte count
    struct Receive {
        Receive(std::size_t size);
        std::size_t size;
    };

    class ClientMessageBuilder : public MessageBase<ClientMessageBuilder> {
    public:

        // IMessage implementation
        virtual void reset() override;
        virtual void start() override;

        ClientMessageBuilder(std::shared_ptr<laar::SharedBuffer> buffer);

        struct MessageData {
            std::size_t size;
            NSound::TClientMessage payload;
        };

        bool constructed();
        std::unique_ptr<MessageData> result();

        bool isInSize();
        bool isInData();
        bool isInTrail();

        class IEventStateHandle : public MessageBase<ClientMessageBuilder>::MessageStateBase {
        public:
            IEventStateHandle(ClientMessageBuilder* data) : MessageStateBase(data) {}
            virtual void event(Receive event) {}
        };

        class Size : IEventStateHandle {
        public:
            Size(ClientMessageBuilder* data)            : IEventStateHandle(data) {}
            // IMessageState implementation
            virtual void entry() override               { bytes_ = sizeof this->fsm_->data_->size; }
            virtual void exit() override                {}
            virtual std::size_t bytes() const override  { return bytes_; }

            void event(Receive event) override;

            friend ClientMessageBuilder;

        private:
            std::size_t bytes_;
        };

        class Data : IEventStateHandle {
        public:
            Data(ClientMessageBuilder* data)            : IEventStateHandle(data) {}
            // IMessageState implementation
            virtual void entry() override               { bytes_ = this->fsm_->data_->size; }
            virtual void exit() override                {}
            virtual std::size_t bytes() const override  { return bytes_; }

            virtual void event(Receive event) override;

            friend ClientMessageBuilder;

        private:
            std::size_t bytes_;
        };

        class Trail : IEventStateHandle {
        public:
            Trail(ClientMessageBuilder* data)           : IEventStateHandle(data) {}
            // IMessageState implementation
            virtual void entry() override               { bytes_ = 0; }
            virtual void exit() override                {}
            virtual std::size_t bytes() const override  { return bytes_; }

            virtual void event(Receive event) override;

            friend ClientMessageBuilder;

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
}
