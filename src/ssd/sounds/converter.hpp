#pragma once

// laar
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>

// RtAudio
#include <RtAudio.h>

// std
#include <cstdint>
#include <format>
#include <memory>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// proto
#include <protos/client/simple/simple.pb.h>

namespace laar {

    using ESampleType = NSound::NSimple::TSimpleMessage::TStreamConfiguration::TSampleSpecification::TFormat;

    std::uint16_t convertToLE(std::uint16_t);
    std::uint16_t convertToBE(std::uint16_t);
    std::uint32_t convertToLE(std::uint32_t);
    std::uint32_t convertToBE(std::uint32_t);

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
    std::int32_t convertFromFloat32BE(std::int32_t sample);
    std::int32_t convertFromFloat32LE(std::int32_t sample);
    std::int32_t convertFromSigned32LE(std::int32_t sample);
    std::int32_t convertFromSigned32BE(std::int32_t sample);
    std::int32_t convertFromSigned16BE(std::uint16_t sample);
    std::int32_t convertFromSigned16BE(std::uint16_t sample);

    std::size_t getSampleSize(ESampleType format);

};