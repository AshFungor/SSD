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

// local
#include "converter.hpp"

std::uint8_t laar::convertToUnsigned8(std::int32_t sample) {
   return ((std::int64_t) sample + INT32_MIN) / UINT8_MAX;
}

std::uint32_t laar::convertToFloat32BE(std::int32_t sample) {
    float floating = (sample > 0) ? ((float) sample / INT32_MAX) : ((float) sample / INT32_MIN);
    return convertToBE(reinterpret_cast<uint32_t&>(floating));
}

std::uint32_t laar::convertToFloat32LE(std::int32_t sample) {
    return convertToLE(convertToFloat32BE(sample));
}

std::uint32_t laar::convertToSigned32LE(std::int32_t sample) {
    return convertToLE(reinterpret_cast<std::uint32_t&>(sample));
}

std::uint32_t laar::convertToSigned32BE(std::int32_t sample) {
    return convertToBE(reinterpret_cast<std::uint32_t&>(sample));
}

std::uint16_t laar::convertToSigned16BE(std::int32_t sample) {
    auto result = (sample > 0) ? (sample / INT16_MAX) : (sample / INT16_MIN);
    return convertToBE(reinterpret_cast<std::uint16_t&>(sample));
}

std::uint16_t laar::convertToSigned16LE(std::int32_t sample) {
    return convertToLE(convertToSigned16BE(sample));
}