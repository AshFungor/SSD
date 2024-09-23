// std
#include <cstdint>

// laar
#include <src/common/macros.hpp>
#include <src/ssd/sound/converter.hpp>

// abseil
#include <absl/base/internal/endian.h>

// proto
#include <protos/client/base.pb.h>

using ESamples = 
    NSound::NClient::NBase::TBaseMessage::TStreamConfiguration::TSampleSpecification;


std::uint16_t laar::convertToLE(std::uint16_t data) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        return data;
    #elif __BYTE_ORDER == __BIG_ENDIAN
        return absl::gbswap_16(data);
    #else
        #error Endianess is unsupported
    #endif
}

std::uint16_t laar::convertToBE(std::uint16_t data) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        return absl::gbswap_16(data);
    #elif __BYTE_ORDER == __BIG_ENDIAN
        return data;
    #else
        #error Endianess is unsupported
    #endif
}

std::uint32_t laar::convertToLE(std::uint32_t data) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        return data;
    #elif __BYTE_ORDER == __BIG_ENDIAN
        return absl::gbswap_32(data);
    #else
        #error Endianess is unsupported
    #endif
}

std::uint32_t laar::convertToBE(std::uint32_t data) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        return absl::gbswap_32(data);
    #elif __BYTE_ORDER == __BIG_ENDIAN
        return data;
    #else
        #error Endianess is unsupported
    #endif
}

std::uint16_t laar::convertFromLE(std::uint16_t data) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        return data;
    #elif __BYTE_ORDER == __BIG_ENDIAN
        return absl::gbswap_16(data);
    #else
        #error Endianess is unsupported
    #endif
}

std::uint16_t laar::convertFromBE(std::uint16_t data) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        return absl::gbswap_16(data);
    #elif __BYTE_ORDER == __BIG_ENDIAN
        return data;
    #else
        #error Endianess is unsupported
    #endif
}

std::uint32_t laar::convertFromLE(std::uint32_t data) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        return data;
    #elif __BYTE_ORDER == __BIG_ENDIAN
        return absl::gbswap_32(data);
    #else
        #error Endianess is unsupported
    #endif
}

std::uint32_t laar::convertFromBE(std::uint32_t data) {
    #if __BYTE_ORDER == __LITTLE_ENDIAN
        return absl::gbswap_32(data);
    #elif __BYTE_ORDER == __BIG_ENDIAN
        return data;
    #else
        #error Endianess is unsupported
    #endif
}

std::uint8_t laar::convertToUnsigned8(std::int32_t sample) {
   return ((std::int64_t) sample - INT32_MIN) / (((std::int64_t) INT32_MAX - INT32_MIN) / UINT8_MAX);
}

std::uint32_t laar::convertToFloat32BE(std::int32_t sample) {
    float floating = (sample > 0) ? ((float) sample / INT32_MAX) : ((float) sample / -INT32_MIN);
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
    constexpr auto positiveScale = (((std::int64_t) INT32_MAX - INT32_MIN) / INT16_MAX);
    constexpr auto negativeScale = (((std::int64_t) INT32_MAX - INT32_MIN) / -INT16_MIN);
    auto result = (sample > 0) ? (sample / positiveScale) : (sample / negativeScale);
    return convertToBE(reinterpret_cast<std::uint16_t&>(result));
}

std::uint16_t laar::convertToSigned16LE(std::int32_t sample) {
    return convertToLE(convertToSigned16BE(sample));
}

std::size_t laar::getSampleSize(ESampleType format) {
    switch(format) {
        case ESamples::UNSIGNED_8:
            return sizeof(std::uint8_t);
        case ESamples::SIGNED_16_BIG_ENDIAN:
        case ESamples::SIGNED_16_LITTLE_ENDIAN:
            return sizeof(std::uint16_t);
        case ESamples::SIGNED_32_LITTLE_ENDIAN:
        case ESamples::SIGNED_32_BIG_ENDIAN:
        case ESamples::FLOAT_32_BIG_ENDIAN:
        case ESamples::FLOAT_32_LITTLE_ENDIAN:
            return sizeof(std::uint32_t);
        default:
            SSD_ABORT_UNLESS(false);
    }
}

std::int32_t laar::convertFromUnsigned8(std::uint8_t sample) {
    return (std::int64_t) sample * (((std::int64_t) INT32_MAX - INT32_MIN) / UINT8_MAX) + INT32_MIN;
}

std::int32_t laar::convertFromFloat32BE(std::uint32_t sample) {
    auto converted = convertFromBE(sample);
    long double floating = reinterpret_cast<float&>(converted);
    return (floating > 0) ? (floating * INT32_MAX) : -(floating * INT32_MIN);
}

std::int32_t laar::convertFromFloat32LE(std::uint32_t sample) {
    auto converted = convertFromLE(sample);
    long double floating = reinterpret_cast<float&>(converted);
    return (floating > 0) ? (floating * INT32_MAX) : -(floating * INT32_MIN);
}

std::int32_t laar::convertFromSigned32LE(std::uint32_t sample) {
    auto converted = convertFromLE(sample);
    return reinterpret_cast<std::int32_t&>(converted);
}

std::int32_t laar::convertFromSigned32BE(std::uint32_t sample) {
    auto converted = convertFromBE(sample);
    return reinterpret_cast<std::int32_t&>(converted);
}

std::int32_t laar::convertFromSigned16BE(std::uint16_t sample) {
    constexpr auto positiveScale =  INT32_MAX / INT16_MAX;
    constexpr auto negativeScale =  INT32_MIN / INT16_MIN;
    auto converted = convertFromBE(sample);
    std::int32_t casted = reinterpret_cast<int16_t&>(converted);
    return (casted > 0) ? (casted * positiveScale) : (casted * negativeScale);
}

std::int32_t laar::convertFromSigned16LE(std::uint16_t sample) {
    constexpr auto positiveScale =  INT32_MAX / INT16_MAX;
    constexpr auto negativeScale =  INT32_MIN / INT16_MIN;
    auto converted = convertFromLE(sample);
    std::int32_t casted = reinterpret_cast<int16_t&>(converted);
    return (casted > 0) ? (casted * positiveScale) : (casted * negativeScale);
}