// STD
#include <istream>
#include <cstdint>

// Plog
#include <plog/Log.h>
#include <plog/Severity.h>

// Abseil
#include <absl/base/internal/endian.h>

// laar
#include <src/ssd/core/header.hpp>

using namespace laar;

namespace {

    // masks
    constexpr std::uint16_t VERSION_MASK     = 0x000F;
    constexpr std::uint16_t TYPE_MASK        = 0xFF00;
    // shifts
    constexpr std::uint16_t VERSION_SHIFT    = 0;
    constexpr std::uint16_t TYPE_SHIFT       = 8;

}

Header Header::readFromStream(std::istream& is) {
    Header header;
    std::uint16_t lookahead;

    // ID
    is.read(reinterpret_cast<char*>(&lookahead), sizeof(lookahead));
    lookahead = absl::gntohs(lookahead);

    header.version_ = (lookahead & VERSION_MASK) >> VERSION_SHIFT;
    header.payloadType_ = (lookahead & TYPE_MASK) >> TYPE_SHIFT;
    // Payload size
    is.read(reinterpret_cast<char*>(header.payloadSize_), sizeof(header.payloadSize_));
    header.payloadSize_ = absl::gntohs(header.payloadSize_);

    if (is.fail()) {
        PLOG(plog::error) << "failed to parse header: error bit on string stream is set to 1";
        std::abort();
    }

    return header;
}

std::uint32_t Header::getPayloadSize() const {
    return payloadSize_;
}

std::uint8_t Header::getProtocolVersion() const {
    return version_;
}

std::uint8_t Header::getPayloadType() const {
    return payloadType_;
}

void Header::toArray(void* data) const {
    std::stringstream oss {reinterpret_cast<char*>(data), std::ios::out | std::ios::binary};
    std::uint16_t writeahead = 0;

    writeahead |= version_ << VERSION_SHIFT;
    writeahead |= payloadType_ << TYPE_SHIFT;
    writeahead = absl::ghtons(writeahead);
    oss.write(reinterpret_cast<char*>(&writeahead), sizeof(writeahead));

    std::uint32_t size = absl::ghtonl(payloadSize_);
    oss.write(reinterpret_cast<char*>(&size), sizeof(size));

    if (oss.fail()) {
        PLOG(plog::error) << "failed to write header: error bit on string stream is set to 1";
        std::abort();
    }
}

Header Header::make(std::uint32_t payloadSize, std::uint8_t version, std::uint8_t payloadType) {
    Header header;
    header.payloadSize_ = payloadSize;
    header.payloadType_ = payloadType;
    header.version_ = version;
    return header;
}
