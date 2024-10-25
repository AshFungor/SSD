#pragma once

// RtAudio
#include <RtAudio.h>

// std
#include <cstdint>

// proto
#include <protos/client/base.pb.h>

namespace laar {

    using ESampleType =
        NSound::NCommon::TStreamConfiguration::TSampleSpecification::TFormat;

    // endianess
    std::uint16_t convertToLE(std::uint16_t data);
    std::uint16_t convertToBE(std::uint16_t data);
    std::uint32_t convertToLE(std::uint32_t data);
    std::uint32_t convertToBE(std::uint32_t data);
    std::uint16_t convertFromLE(std::uint16_t data);
    std::uint16_t convertFromBE(std::uint16_t data);
    std::uint32_t convertFromLE(std::uint32_t data);
    std::uint32_t convertFromBE(std::uint32_t data);

    // for pulling
    std::uint8_t convertToUnsigned8(std::int32_t sample);
    std::uint32_t convertToFloat32BE(std::int32_t sample);
    std::uint32_t convertToFloat32LE(std::int32_t sample);
    std::uint32_t convertToSigned32LE(std::int32_t sample);
    std::uint32_t convertToSigned32BE(std::int32_t sample);
    std::uint16_t convertToSigned16BE(std::int32_t sample);
    std::uint16_t convertToSigned16LE(std::int32_t sample);

    // for pushing
    std::int32_t convertFromUnsigned8(std::uint8_t sample);
    std::int32_t convertFromFloat32BE(std::uint32_t sample);
    std::int32_t convertFromFloat32LE(std::uint32_t sample);
    std::int32_t convertFromSigned32LE(std::uint32_t sample);
    std::int32_t convertFromSigned32BE(std::uint32_t sample);
    std::int32_t convertFromSigned16BE(std::uint16_t sample);
    std::int32_t convertFromSigned16LE(std::uint16_t sample);

    // get sample size on bytes
    std::size_t getSampleSize(ESampleType format);

};