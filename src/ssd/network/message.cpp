// laar
#include <common/exceptions.hpp>
#include <common/macros.hpp>

// plog
#include <memory>
#include <plog/Log.h>
#include <plog/Severity.h>

// STD
#include <cstdint>
#include <string>

// local
#include "message.hpp"
#include "protos/client/client-message.pb.h"

#define ENSURE_STATE(current, target) \
    SSD_ABORT_UNLESS(current == target)

using namespace laar;

void MessageBuilder::reset() {
    stage_ = EState::IN_HEADER;
    requested_ = headerSize_;
}

void MessageBuilder::init() {
    // Do nothing
}

std::uint32_t MessageBuilder::poll() const {
    return requested_;
}

bool MessageBuilder::handle(Receive payload) {
    if (payload.size < requested_ || !payload.size) {
        requested_ -= payload.size;
        return ready();
    }

    if (payload.size > requested_) {
        onCriticalError(laar::LaarOverrun(payload.size - requested_));
    }

    requested_ = 0;
    switch(stage_) {
        case EState::IN_HEADER:
            handleHeader();
            return ready();
        case EState::IN_PAYLOAD:
            handlePayload();
            return ready();
        case EState::OUT_READY:
            onCriticalError(laar::LaarSemanticError());
    }

    return ready();
}

void MessageBuilder::handleHeader() {
    ENSURE_STATE(stage_, EState::IN_HEADER);

    std::size_t header = 0;
    buffer_->read((char*) &header, headerSize_);

    // version
    current_.version = static_cast<IResult::EVersion>(header & versionMask);
    if ((std::size_t) current_.version > (std::size_t) IResult::EVersion::FIRST) {
        fallback(laar::LaarProtocolError(
            "version is unsupported, value received: " + std::to_string((std::size_t) current_.version)));
    }

    current_.payloadType = ((header & isRawMask) >> 4) ? IResult::EPayloadType::RAW : IResult::EPayloadType::STRUCTURED;
    current_.messageType = static_cast<IResult::EType>((header & messageTypeMask) >> 8);

    if ((std::size_t) current_.messageType > (std::size_t) IResult::EType::FLUSH_SIMPLE) {
        fallback(laar::LaarProtocolError(
            "message type is unsupported, value received: " + std::to_string((std::size_t) current_.messageType)));
    }

    current_.payloadSize = (header & payloadSizeMask) >> 16;

    // next state
    stage_ = EState::IN_PAYLOAD;
    requested_ = current_.payloadSize;
}

void MessageBuilder::handlePayload() {
    ENSURE_STATE(stage_, EState::IN_PAYLOAD);

    switch (current_.payloadType) {
        case IResult::EPayloadType::RAW:
            assembled_ = assembleRaw();
            return;
        case IResult::EPayloadType::STRUCTURED:
            assembled_ = assembleStructured();
            return;
    }

    // next state
    stage_ = EState::OUT_READY;
    requested_ = 0;
}

std::unique_ptr<MessageBuilder::IRawResult> MessageBuilder::assembleRaw() {
    auto payload = std::make_unique<MessageBuilder::RawResult>(
        buffer_, current_.payloadSize, current_.messageType, current_.version
    );
    return std::move(payload);
}

std::unique_ptr<MessageBuilder::IStructuredResult> MessageBuilder::assembleStructured() {
    NSound::TClientMessage message;
    message.ParseFromArray(buffer_->rdCursor(), current_.payloadSize);
    buffer_->drop(current_.payloadSize);

    auto payload = std::make_unique<MessageBuilder::StructuredResult>(
        std::move(message), current_.payloadSize, current_.messageType, current_.version
    );
    return std::move(payload);
}

void MessageBuilder::onCriticalError(std::exception error) {
    PLOG(plog::error) << "error while constructing message: " << error.what();
    throw error;
}

MessageBuilder::RawResult::RawResult(
    std::weak_ptr<RingBuffer> buffer, 
    std::uint32_t size,
    EType type,
    EVersion version)
    : hasPayload_(true)
    , buffer_(std::move(buffer))
    , size_(size)
    , type_(type)
    , version_(version)
{}