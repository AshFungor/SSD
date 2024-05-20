// laar
#include <common/exceptions.hpp>
#include <common/macros.hpp>

// plog
#include <memory>
#include <plog/Log.h>
#include <plog/Severity.h>

// STD
#include <cstdint>
#include <sstream>
#include <string>

// local
#include "message.hpp"
#include "common/plain-buffer.hpp"
#include "network/interfaces/i-message.hpp"
#include "protos/client/client-message.pb.h"

#define ENSURE_STATE(current, target) \
    SSD_ABORT_UNLESS(current == target)

using namespace laar;

namespace {

    std::string dumpHeader(
        IResult::EVersion version,
        IResult::EType messageType,
        IResult::EPayloadType payloadType,
        std::uint32_t payloadSize
    ) {
        std::stringstream ss;
        ss << "Protocol Version: ";
        switch (version) {
            case IResult::EVersion::FIRST:
                ss << "First";
                break;
            default:
                ss << "Undefined";
        }
        ss << "; Payload Type: ";
        switch (payloadType) {
            case IResult::EPayloadType::RAW:
                ss << "RAW";
                break;
            case IResult::EPayloadType::STRUCTURED:
                ss << "STRUCTURED";
                break;
        }
        ss << "; Message Type: ";
        switch (messageType) {
            case IResult::EType::CLOSE_SIMPLE:
                ss << "Simple protocol: close";
                break;
            case IResult::EType::DRAIN_SIMPLE:
                ss << "Simple protocol: drain";
                break;
            case IResult::EType::FLUSH_SIMPLE:
                ss << "Simple protocol: flush";
                break;
            case IResult::EType::OPEN_SIMPLE:
                ss << "Simple protocol: open";
                break;
            case IResult::EType::PUSH_SIMPLE:
                ss << "Simple protocol: push";
                break;
            case IResult::EType::PULL_SIMPLE:
                ss << "Simple protocol: pull";
                break;   
            default:
                ss << "Undefined";
        }
        ss << ";";
        return ss.str();
    }

}

MessageBuilder::MessageBuilder(std::shared_ptr<laar::PlainBuffer> buffer, Private access) 
: IMessageReceiver(std::make_unique<RollbackHandler>())
, buffer_(buffer)
{
    reset();
}

std::shared_ptr<MessageBuilder> MessageBuilder::configure(std::shared_ptr<laar::PlainBuffer> buffer) {
    return std::make_shared<MessageBuilder>(std::move(buffer), Private{});
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
        PLOG(plog::debug) << "requested: " << requested_ << "; size: " << payload.size;
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

std::unique_ptr<IResult> MessageBuilder::fetch() {
    ENSURE_STATE(stage_, EState::OUT_READY);

    reset();
    return std::move(assembled_);
}

bool MessageBuilder::valid() const {
    return stage_ != EState::INVALID;
}

void MessageBuilder::invalidate() {
    stage_ = EState::INVALID;
}

void MessageBuilder::handleHeader() {
    ENSURE_STATE(stage_, EState::IN_HEADER);

    std::size_t header = 0;
    auto received = buffer_->read((char*) &header, headerSize_);

    if (received < headerSize_) {
        onCriticalError(laar::LaarDryRun(headerSize_ - received));
    }

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

    PLOG(plog::debug) << "protocol: raw header received: " << header;
    PLOG(plog::debug) << "protocol: header; data: " 
        << dumpHeader(current_.version, current_.messageType, current_.payloadType, current_.payloadSize);

    // next state
    stage_ = EState::IN_PAYLOAD;
    requested_ = current_.payloadSize;
}

void MessageBuilder::handlePayload() {
    ENSURE_STATE(stage_, EState::IN_PAYLOAD);

    switch (current_.payloadType) {
        case IResult::EPayloadType::RAW:
            assembled_ = assembleRaw();
            break;
        case IResult::EPayloadType::STRUCTURED:
            assembled_ = assembleStructured();
            break;
    }

    // next state
    stage_ = EState::OUT_READY;
    requested_ = 0;
}

std::unique_ptr<IRawResult> MessageBuilder::assembleRaw() {
    ENSURE_STATE(stage_, EState::IN_PAYLOAD);

    auto payload = std::make_unique<MessageBuilder::RawResult>(
        buffer_, current_.payloadSize, current_.messageType, current_.version, current_.payloadType
    );
    return std::move(payload);
}

std::unique_ptr<IStructuredResult> MessageBuilder::assembleStructured() {
    ENSURE_STATE(stage_, EState::IN_PAYLOAD);

    NSound::TClientMessage message;
    auto result = message.ParseFromArray(buffer_->rPosition(), current_.payloadSize);
    buffer_->advance(PlainBuffer::EPosition::RPOS, current_.payloadSize);

    auto payload = std::make_unique<MessageBuilder::StructuredResult>(
        std::move(message), current_.payloadSize, current_.messageType, current_.version, current_.payloadType
    );

    PLOG(plog::debug) << "protocol: payload; data: " << "proto of size " 
        << current_.payloadSize << "bytes, parse result: " << result;

    return std::move(payload);
}

void MessageBuilder::onCriticalError(std::exception error) {
    PLOG(plog::error) << "error while constructing message: " << error.what();
    throw error;
}

MessageBuilder::RawResult::RawResult(
    std::shared_ptr<PlainBuffer> buffer, 
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