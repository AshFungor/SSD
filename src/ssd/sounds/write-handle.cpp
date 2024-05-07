// laar
#include <sounds/interfaces/i-audio-handler.hpp>
#include <sounds/audio-handler.hpp> 
#include <common/ring-buffer.hpp>
#include <common/exceptions.hpp>
#include <sounds/converter.hpp>

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
#include "write-handle.hpp"

using namespace laar;

namespace {

    constexpr std::size_t threshold = 16;

    std::size_t getBufferSize(std::size_t proposed, std::size_t minRequested) {
        if (proposed < minRequested || proposed / 1024 > threshold) {
            return minRequested;
        }
        return proposed;
    }

}

WriteHandle::WriteHandle(TSimpleMessage::TStreamConfiguration config, std::weak_ptr<IListener> owner) 
: sampleSize_(getSampleSize(config.sample_spec().format()))
, format_(config.sample_spec().format())
, buffer_(std::make_unique<laar::RingBuffer>(44100 * 4 * 120)) // think of config here
, owner_(std::move(owner))
, bufferConfig_(config.buffer_config())
{}

int WriteHandle::flush() {
    std::unique_lock<std::mutex> locked(lock_);

    buffer_->drop(buffer_->readableSize());
    return status::SUCCESS;
}

int WriteHandle::read(std::int32_t* dest, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    if (buffer_->readableSize() / sizeof (std::int32_t) < bufferConfig_.prebuffing_size()) {
        PLOG(plog::debug) << "handle " << this << " is stalled, "
            << "waiting for " << bufferConfig_.prebuffing_size() - buffer_->readableSize() / sizeof (std::int32_t)
            << " samples (prebuffing size is " << bufferConfig_.prebuffing_size() 
            << "; readable size is " << buffer_->readableSize() / sizeof (std::int32_t) << ")";
        return status::STALLED;
    } else {
        bufferConfig_.set_prebuffing_size(0);
    }

    std::size_t trail = 0;
    if (size > buffer_->readableSize() / sizeof (std::int32_t)) {
        trail = size - buffer_->readableSize() / sizeof (std::int32_t);
    }

    if (trail) {
        PLOG(plog::debug) << "underrun on handle: " << this
            << " filling " << trail << " extra samples, avail: " << size - trail;
    }

    for (std::size_t frame = 0; frame < size - trail; ++frame) {
        buffer_->read((char*) (dest + frame), sizeof(std::int32_t));
    }

    for (std::size_t frame = size - trail; frame < size; ++frame) {
        std::memcpy(dest + frame, &Silence, sizeof(std::int32_t));
    }

    if (trail) {
        return status::UNDERRUN;
    }
    return status::SUCCESS;
}

int WriteHandle::write(const char* src, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    std::uint8_t sample8 = 0;
    std::uint16_t sample16 = 0;
    std::uint16_t sample32 = 0;
    std::int32_t converted = 0;

    bool overrun = false;
    if (size > buffer_->writableSize()) {
        PLOG(plog::warning) << "overrun on handle: " << this 
            << "; cutting " << size - buffer_->writableSize() << " samples on stream";
        size = buffer_->writableSize();
        overrun = true;
    }

    for (std::size_t frame = 0; frame < size; ++frame) {
        if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::UNSIGNED_8) {
            std::memcpy(&sample8, src + frame, sizeof(sample8));
            converted = convertFromUnsigned8(sample8);
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_BIG_ENDIAN) {
            std::memcpy(&sample16, src + frame, sizeof(sample16));
            converted = convertFromSigned16BE(sample16);
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_LITTLE_ENDIAN) {
            std::memcpy(&sample16, src + frame, sizeof(sample16));
            converted = convertFromSigned16LE(sample16);
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_BIG_ENDIAN) {
            std::memcpy(&sample32, src + frame, sizeof(sample32));
            converted = convertFromFloat32BE(sample32);
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_LITTLE_ENDIAN) {
            std::memcpy(&sample32, src + frame, sizeof(sample32));
            converted = convertFromFloat32LE(sample32);
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_BIG_ENDIAN) {
            std::memcpy(&sample32, src + frame, sizeof(sample32));
            converted = convertFromSigned32BE(sample32);
        } else if (format_ == TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN) {
            std::memcpy(&sample32, src + frame, sizeof(sample32));
            converted = convertFromSigned32LE(sample32);
        } else {
            throw laar::LaarSoundHandlerError("sample type is not supported");
        }
        buffer_->write((char*) &converted, sizeof(std::uint32_t));
    }

    if (overrun) {
        return status::OVERRUN;
    }
    return status::SUCCESS;
}

ESampleType WriteHandle::format() const {
    return format_;
}