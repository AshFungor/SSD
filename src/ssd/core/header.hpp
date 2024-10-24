#pragma once

// STD
#include <memory>
#include <istream>
#include <cstdint>

namespace laar {

    namespace protocol {

        namespace version {
            inline constexpr std::uint8_t FIRST = 0x01;
        }

        namespace payload_type {
            inline constexpr std::uint8_t RAW = 0x01;
            inline constexpr std::uint8_t PROTOBUF = 0x02;
        }

    }

    // protocol header
    class Header {
    public:

        static Header readFromStream(std::istream& is);
        static Header make(
            std::uint32_t payloadSize, 
            std::uint8_t version = protocol::version::FIRST,
            std::uint8_t payloadType = protocol::payload_type::PROTOBUF
        );

        inline static constexpr std::size_t getHeaderSize() {
            return headerSize_;
        }

        std::unique_ptr<std::uint8_t[]> buffer() const;

        // getters
        std::uint32_t getPayloadSize() const;
        std::uint8_t getProtocolVersion() const;
        std::uint8_t getPayloadType() const;

    private:
        std::uint8_t version_ = 0;
        std::uint8_t payloadType_ = 0;
        std::uint32_t payloadSize_ = 0;

        static constexpr std::size_t headerSize_ = 
            sizeof(payloadSize_) + sizeof(version_) + sizeof(payloadType_);
    };

}