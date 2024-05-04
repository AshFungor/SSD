#pragma once

// laar
#include <sounds/interfaces/i-audio-handler.hpp>
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>
#include <sounds/converter.hpp>
#include <sounds/audio-handler.hpp>

// RtAudio
#include <RtAudio.h>

// std
#include <cstdint>
#include <memory>
#include <mutex>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// proto
#include <protos/client/simple/simple.pb.h>

// local
#include "read-handle.hpp"

using namespace laar;

using TSampleSpec = TSimpleMessage::TStreamConfiguration::TSampleSpecification;

ReadHandle::ReadHandle(TSimpleMessage::TStreamConfiguration config, std::weak_ptr<IListener> owner) 
: sampleSize_(getSampleSize(config.sample_spec().format()))
, format_(config.sample_spec().format())
, owner_(std::move(owner))
{}

int ReadHandle::flush() {
    std::unique_lock<std::mutex> locked(lock_);

    buffer_->drop(buffer_->readableSize());
    return status::SUCCESS;
}

int ReadHandle::read(char* dest, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    std::int32_t baseSample; 

    std::size_t trail = size - buffer_->readableSize() / 4;

    if (trail) {
        PLOG(plog::warning) << "underrun on handle: " << this
            << " filling " << trail << " extra samples";
    }

    for (std::size_t frame = 0; frame < size - trail; ++frame) {
        buffer_->read((char*) &baseSample, sizeof(std::int32_t));
        if (format_ == TSampleSpec::UNSIGNED_8) {
            auto converted = convertToUnsigned8(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::SIGNED_16_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::SIGNED_16_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::FLOAT_32_BIG_ENDIAN) {
            auto converted = convertToFloat32BE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::FLOAT_32_LITTLE_ENDIAN) {
            auto converted = convertToFloat32LE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::SIGNED_32_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::SIGNED_32_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else {
            throw laar::LaarSoundHandlerError("sample type is not supported");
        }
    }

    for (std::size_t frame = size - trail; frame < size; ++frame) {
        if (format_ == TSampleSpec::UNSIGNED_8) {
            auto converted = convertToUnsigned8(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::SIGNED_16_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::SIGNED_16_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::FLOAT_32_BIG_ENDIAN) {
            auto converted = convertToFloat32BE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::FLOAT_32_LITTLE_ENDIAN) {
            auto converted = convertToFloat32LE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::SIGNED_32_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == TSampleSpec::SIGNED_32_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else {
            throw laar::LaarSoundHandlerError("sample type is not supported");
        }
    }

    if (trail) {
        // underrun
        return status::UNDERRUN;
    }

    return status::SUCCESS;
}

int ReadHandle::write(std::int32_t* src, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    bool overrun = false;
    if (size > buffer_->writableSize()) {
        PLOG(plog::warning) << "overrun on handle: " << this 
            << "; cutting " << size - buffer_->writableSize() << " samples on stream";
        size = buffer_->writableSize();
        overrun = true;
    }

    for (std::size_t frame = 0; frame < size; ++frame) {
        buffer_->write((char*) (src + frame), sizeof(std::int32_t));
    }

    if (overrun) {
        // overrun
        return status::OVERRUN;
    }

    return status::SUCCESS;
}

ESampleType ReadHandle::format() const {
    return format_;
}