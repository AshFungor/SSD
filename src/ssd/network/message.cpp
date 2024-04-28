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

MessageBuilder::MessageBuilder(std::shared_ptr<laar::RingBuffer> buffer, Private access) 
: buffer_(buffer)
{
    reset();
}

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
        case EState::INVALID:
            onCriticalError(laar::LaarSemanticError());
    }

    return ready();
}

MessageBuilder::operator bool() const {
    return ready();
}

bool MessageBuilder::ready() const {
    return stage_ == EState::OUT_READY;
}

std::unique_ptr<MessageBuilder::IResult> MessageBuilder::fetch() {
    ENSURE_STATE(stage_, EState::OUT_READY);

    return std::move(assembled_);
}

bool MessageBuilder::valid() const {
    return stage_ != EState::INVALID;
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
    ENSURE_STATE(stage_, EState::IN_PAYLOAD);

    auto payload = std::make_unique<MessageBuilder::RawResult>(
        buffer_, current_.payloadSize, current_.messageType, current_.version, current_.payloadType
    );
    return std::move(payload);
}

std::unique_ptr<MessageBuilder::IStructuredResult> MessageBuilder::assembleStructured() {
    ENSURE_STATE(stage_, EState::IN_PAYLOAD);

    NSound::TClientMessage message;
    message.ParseFromArray(buffer_->rdCursor(), current_.payloadSize);
    buffer_->drop(current_.payloadSize);

    auto payload = std::make_unique<MessageBuilder::StructuredResult>(
        std::move(message), current_.payloadSize, current_.messageType, current_.version, current_.payloadType
    );
    return std::move(payload);
}

void MessageBuilder::onCriticalError(std::exception error) {
    PLOG(plog::error) << "error while constructing message: " << error.what();
    throw error;
}

MessageBuilder::RawResult::RawResult(
    std::shared_ptr<RingBuffer> buffer, 
    std::uint32_t size,
    EType type,
    EVersion version,
    EPayloadType payloadType)
    : buffer_(size)
    , size_(size)
    , type_(type)
    , version_(version)
    , payloadType_(payloadType)
{
    buffer->read(buffer_.data(), size);
}

MessageBuilder::StructuredResult::StructuredResult(
    NSound::TClientMessage message, 
    std::uint32_t size,
    EType type,
    EVersion version,
    EPayloadType payloadType)
    : message_(std::move(message))
    , size_(size)
    , type_(type)
    , version_(version)
    , payloadType_(payloadType)
{}

char* MessageBuilder::RawResult::payload() {
    return buffer_.data();
}

NSound::TClientMessage& MessageBuilder::StructuredResult::payload() {
    return message_;
}

std::uint32_t MessageBuilder::RawResult::size() const {
    return size_;
}

std::uint32_t MessageBuilder::StructuredResult::size() const {
    return size_;
}

MessageBuilder::RawResult::EVersion MessageBuilder::RawResult::version() const {
    return version_;
}

MessageBuilder::StructuredResult::EVersion MessageBuilder::StructuredResult::version() const {
    return version_;
}

MessageBuilder::RawResult::EType MessageBuilder::RawResult::type() const {
    return type_;
}

MessageBuilder::StructuredResult::EType MessageBuilder::StructuredResult::type() const {
    return type_;
}

MessageBuilder::RawResult::EPayloadType MessageBuilder::RawResult::payloadType() const {
    return payloadType_;
}

MessageBuilder::StructuredResult::EPayloadType MessageBuilder::StructuredResult::payloadType() const {
    return payloadType_;
}