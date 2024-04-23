// laar
#include <common/exceptions.hpp>

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

ClientMessageBuilder::ClientMessageBuilder(std::shared_ptr<laar::SharedBuffer> buffer)
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
    if (event.size > bytes_) {
        throw laar::LaarOverrun(event.size - bytes_);
    }

    bytes_ -= event.size;
    if (!bytes_) {
        std::memcpy(
            &fsm_->data_->size, 
            fsm_->buffer_->getRawReadCursor(), 
            sizeof(fsm_->data_->size)
        );
        transition(&fsm_->dataState_);
    }
}

void ClientMessageBuilder::Data::event(Receive event) {
    if (event.size > bytes_) {
        throw laar::LaarOverrun();
    }

    bytes_ -= event.size;
    if (!bytes_) {
        auto success = fsm_->data_->payload.ParseFromArray(fsm_->buffer_->getRawReadCursor(), fsm_->data_->size);
        if (!success) {
            throw laar::LaarBadReceive();
        }
        transition(&fsm_->trailState_);
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