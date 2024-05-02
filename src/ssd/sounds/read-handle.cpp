// laar
#include <cstdint>
#include <mutex>
#include <sounds/interfaces/i-audio-handler.hpp>
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>

// RtAudio
#include <RtAudio.h>

// std
#include <format>
#include <memory>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// proto
#include <protos/client/simple/simple.pb.h>

// local
#include "read-handle.hpp"
#include "sounds/converter.hpp"

using namespace laar;

ReadHandle::ReadHandle(TSimpleMessage::TStreamConfiguration config) 
: sampleSize_(getSampleSize(config.sample_spec().format()))
, format_(config.sample_spec().format())
{}

void ReadHandle::flush() {
    std::unique_lock<std::mutex> locked(lock_);

    buffer_->drop(buffer_->readableSize());
}

void ReadHandle::read(char* dest, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    std::int32_t baseSample; 
    for (std::size_t frame = 0; frame < size; ++frame) {
        buffer_->read((char*) &baseSample, sizeof(std::int32_t));
        if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::UNSIGNED_8) {
            auto converted = convertToUnsigned8(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_BIG_ENDIAN) {
            auto converted = convertToFloat32BE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_LITTLE_ENDIAN) {
            auto converted = convertToFloat32LE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else {
            throw laar::LaarSoundHandlerError("sample type is not supported");
        }
    }
}

void ReadHandle::write(std::int32_t* src, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    for (std::size_t frame = 0; frame < size; ++frame) {
        buffer_->write((char*) (src + frame), sizeof(std::int32_t));
    }
}

ESampleType ReadHandle::format() const {
    return format_;
}