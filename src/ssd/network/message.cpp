// laar
#include <common/exceptions.hpp>
#include <plog/Log.h>
#include <plog/Severity.h>

// local
#include "message.hpp"

using namespace laar;


Receive::Receive(std::size_t size)
: size(size)
{}

void ClientMessageBuilder::reset() {
    data_ = std::make_unique<ClientMessageBuilder::MessageData>();
    current_ = &sizeState_;
    current_->entry();
}


void ClientMessageBuilder::start() {
    reset();
}

ClientMessageBuilder::ClientMessageBuilder(std::shared_ptr<laar::RingBuffer> buffer)
: MessageBase<ClientMessageBuilder>({&sizeState_, &dataState_, &trailState_}, &sizeState_)
, buffer_(buffer)
{}

bool ClientMessageBuilder::constructed() {
    return isInTrail();
}

std::unique_ptr<typename ClientMessageBuilder::MessageData> ClientMessageBuilder::result() {
    if (constructed()) {
        return std::move(data_);
    }
    throw laar::LaarBadGet();
}

void ClientMessageBuilder::Size::event(Receive event) {
    auto& payload = message_->data_;
    auto& buffer = message_->buffer_;

    if (event.size > bytes_) {
        throw laar::LaarOverrun(event.size - bytes_);
    }

    bytes_ -= event.size;
    if (!bytes_) {
        std::memcpy(&payload->size, buffer->rdCursor(), sizeof(payload->size));
        buffer->drop(sizeof(payload->size));
        transition(&message_->dataState_);
    }
}

void ClientMessageBuilder::Data::event(Receive event) {
    auto& payload = message_->data_;
    auto& buffer = message_->buffer_;

    if (event.size > bytes_) {
        throw laar::LaarOverrun();
    }

    bytes_ -= event.size;
    if (!bytes_) {
        auto success = payload->payload.ParseFromArray(buffer->rdCursor(), payload->size);
        buffer->drop(payload->size);
        if (!success) {
            PLOG(plog::warning) << "Unable to parse message payload, reseting state";
            throw laar::LaarBadReceive();
        }
        transition(&message_->trailState_);
    }
}

void ClientMessageBuilder::Trail::event(Receive event) {
    if (event.size) {
        throw laar::LaarOverrun();
    }
}

bool ClientMessageBuilder::isInSize() {
    return (void*) current_ == (void*) &sizeState_;
}

bool ClientMessageBuilder::isInData() {
    return (void*) current_ == (void*) &dataState_;
}

bool ClientMessageBuilder::isInTrail() {
    return (void*) current_ == (void*) &trailState_;
}