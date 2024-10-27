// STD
#include "protos/server-message.pb.h"
#include "src/ssd/macros.hpp"
#include <cstddef>
#include <istream>
#include <cstdint>

// Plog
#include <plog/Log.h>
#include <plog/Severity.h>

// Abseil
#include <absl/base/internal/endian.h>

// laar
#include <src/ssd/core/message.hpp>

using namespace laar;

namespace {

    // masks
    constexpr std::uint16_t VERSION_MASK     = 0x00FF;
    constexpr std::uint16_t TYPE_MASK        = 0xFF00;
    // shifts
    constexpr std::uint16_t VERSION_SHIFT    = 0;
    constexpr std::uint16_t TYPE_SHIFT       = 8;

    template<typename Integer>
    void readDataFromArray(Integer& integer, void* data) {
        std::memcpy(&integer, data, sizeof(integer));
        if constexpr (sizeof(integer) <= sizeof(std::uint16_t)) {
            absl::gntohs(integer);
        } else if constexpr (sizeof(integer) <= sizeof(std::uint32_t)) {
            absl::gntohl(integer);
        } else if constexpr (sizeof(integer) <= sizeof(std::uint64_t)) {
            absl::gntohll(integer);
        } else {
            static_assert(false, "failed to find suitable conversion function");
        }
    }

    template<typename Integer>
    void writeDataToArray(Integer integer, void* data) {
        if constexpr (sizeof(integer) <= sizeof(std::uint16_t)) {
            absl::ghtons(integer);
        } else if constexpr (sizeof(integer) <= sizeof(std::uint32_t)) {
            absl::ghtonl(integer);
        } else if constexpr (sizeof(integer) <= sizeof(std::uint64_t)) {
            absl::ghtonll(integer);
        } else {
            static_assert(false, "failed to find suitable conversion function");
        }
        std::memcpy(data, &integer, sizeof(integer));
    }

    MessageProtobufPayloadType readProtobufMessage(void* data, std::size_t size) {
        MessageProtobufPayloadType message;
        if (message.ParseFromArray(data, size)) {
            ENSURE_FAIL();
        }
        return message;
    }

    MessageSimplePayloadType readSimpleMessage(void* data, std::size_t size) {
        UNUSED(size);

        MessageSimplePayloadType message;
        readDataFromArray(message, data);
        return message;
    }

    void writeProtobufMessage(void* data, std::size_t size, MessageProtobufPayloadType message) {
        if (message.SerializeToArray(data, size)) {
            ENSURE_FAIL();
        }
    }

    void writeSimpleMessage(void* data, std::size_t size, MessageSimplePayloadType message) {
        UNUSED(size);

        writeDataToArray(message, data);
    }

}

// --- Message ---
void Message::readFromArray(void* data, std::size_t maxBytes) {
    std::size_t shift = 0;
    auto array = reinterpret_cast<std::uint8_t*>(data);

    readHeaderFromArray(array + shift, maxBytes - shift);
    shift += Message::Size::header();
    readVariableFromArray(array + shift, maxBytes - shift);
    shift += Message::Size::variable(this);
    readPayloadFromArray(array + shift, maxBytes - shift);
}

void Message::readHeaderFromArray(void* data, std::size_t size) {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_OR_EQUAL_THAN(Message::Size::header(), size);
#else
    UNUSED(size);
#endif

    readDataFromArray(header_, data);

#ifdef SSD_RUNTIME_CHECKS
    // unimplemented fallback
    switch (version()) {
        case message::version::FIRST:
            goto type;
        default:
            ENSURE_FAIL();
    }

    type:
    switch(type()) {
        case message::type::PROTOBUF:
            goto end;
        case message::type::SIMPLE:
            goto end;
        default:
            ENSURE_FAIL();
    }

    end:
    return;
#endif
}

void Message::readVariableFromArray(void* data, std::size_t size) {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_OR_EQUAL_THAN(Message::Size::variable(this), size);
#else
    UNUSED(size);
#endif

    switch (type()) {
        case message::type::PROTOBUF:
            readDataFromArray(size_, data);
        case message::type::SIMPLE:
            size_ = sizeof(MessageSimplePayloadType);
        default:
            return;
    }
}

void Message::readPayloadFromArray(void* data, std::size_t size) {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_OR_EQUAL_THAN(Message::Size::payload(this), size);
#else
    UNUSED(size);
#endif

    switch(type()) {
        case message::type::PROTOBUF:
            payload_ = readProtobufMessage(data, size);
        case message::type::SIMPLE:
            payload_ = readSimpleMessage(data, size);
    }
}

void Message::writeToArray(void* data, std::size_t maxBytes) const {
    std::size_t shift = 0;
    auto array = reinterpret_cast<std::uint8_t*>(data);

    writeHeaderToArray(array + shift, maxBytes - shift);
    shift += Message::Size::header();
    writeVariableToArray(array + shift, maxBytes - shift);
    shift += Message::Size::variable(this);
    writePayloadToArray(array + shift, maxBytes - shift);
}

void Message::writeHeaderToArray(void* data, std::size_t size) const {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_OR_EQUAL_THAN(Message::Size::header(), size);
#else
    UNUSED(size);
#endif

    writeDataToArray(header_, data);
}

void Message::writeVariableToArray(void* data, std::size_t size) const {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_OR_EQUAL_THAN(Message::Size::variable(this), size);
#else
    UNUSED(size);
#endif

    switch (type()) {
        case message::type::PROTOBUF:
            writeDataToArray(size_, data);
        case message::type::SIMPLE:
            // fallthrough
        default:
            return;
    }
}

void Message::writePayloadToArray(void* data, std::size_t size) const {
#ifdef SSD_RUNTIME_CHECKS
    ENSURE_NOT_NULL(data);
    ENSURE_SIZE_IS_LESS_OR_EQUAL_THAN(Message::Size::payload(this), size);
#else
    UNUSED(size);
#endif

    switch(type()) {
        case message::type::PROTOBUF:
            writeProtobufMessage(data, size, std::get<MessageProtobufPayloadType>(payload_));
        case message::type::SIMPLE:
            writeSimpleMessage(data, size, std::get<MessageSimplePayloadType>(payload_));
    }
}

// Header Header::readFromArray(std::istream& is) {
//     Header header;
//     std::uint16_t lookahead;

//     // ID
//     is.read(reinterpret_cast<char*>(&lookahead), sizeof(lookahead));
//     lookahead = absl::gntohs(lookahead);

//     header.version_ = (lookahead & VERSION_MASK) >> VERSION_SHIFT;
//     header.payloadType_ = (lookahead & TYPE_MASK) >> TYPE_SHIFT;
//     // Payload size
//     is.read(reinterpret_cast<char*>(header.payloadSize_), sizeof(header.payloadSize_));
//     header.payloadSize_ = absl::gntohs(header.payloadSize_);

//     if (is.fail()) {
//         PLOG(plog::error) << "failed to parse header: error bit on string stream is set to 1";
//         std::abort();
//     }

//     return header;
// }

// std::uint32_t Header::getPayloadSize() const {
//     return payloadSize_;
// }

// std::uint8_t Header::getProtocolVersion() const {
//     return version_;
// }

// std::uint8_t Header::getPayloadType() const {
//     return payloadType_;
// }

// void Header::writeToArray(void* data) const {
//     std::stringstream oss {reinterpret_cast<char*>(data), std::ios::out | std::ios::binary};
//     std::uint16_t writeahead = 0;

//     writeahead |= version_ << VERSION_SHIFT;
//     writeahead |= payloadType_ << TYPE_SHIFT;
//     writeahead = absl::ghtons(writeahead);
//     oss.write(reinterpret_cast<char*>(&writeahead), sizeof(writeahead));

//     std::uint32_t size = absl::ghtonl(payloadSize_);
//     oss.write(reinterpret_cast<char*>(&size), sizeof(size));

//     if (oss.fail()) {
//         PLOG(plog::error) << "failed to write header: error bit on string stream is set to 1";
//         std::abort();
//     }
// }

// Header Header::make(std::uint32_t payloadSize, std::uint8_t version, std::uint8_t payloadType) {
//     Header header;
//     header.payloadSize_ = payloadSize;
//     header.payloadType_ = payloadType;
//     header.version_ = version;
//     return header;
// }
