// abseil
#include <absl/status/status.h>
#include <absl/status/statusor.h>

// laar
#include <src/common/exceptions.hpp>
#include <src/common/ring-buffer.hpp>
#include <src/ssd/sound/converter.hpp>
#include <src/ssd/sound/read-handle.hpp>
#include <src/common/callback-queue.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// Plog
#include <plog/Log.h>

// std
#include <memory>

// proto
#include <protos/client/base.pb.h>

using namespace laar;

using ESamples = 
    NSound::NClient::NBase::TBaseMessage::TStreamConfiguration::TSampleSpecification;


ReadHandle::ReadHandle(
    NSound::NClient::NBase::TBaseMessage::TStreamConfiguration config, 
    std::weak_ptr<IListener> owner
) 
    : format_(config.samplespec().format())
    , sampleSize_(getSampleSize(config.samplespec().format()))
    , owner_(std::move(owner))
{}

absl::Status ReadHandle::flush() {
    std::unique_lock<std::mutex> locked(lock_);

    buffer_->drop(buffer_->readableSize());
    return absl::OkStatus();
}

absl::StatusOr<int> ReadHandle::read(char* dest, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    std::int32_t baseSample; 
    std::size_t trail = size - buffer_->readableSize() / BaseSampleSize;

    if (trail) {
        PLOG(plog::warning) << "underrun on handle: " << this
            << " filling " << trail << " extra samples";
    }

    for (std::size_t frame = 0; frame < size - trail; ++frame) {
        buffer_->read((char*) &baseSample, sizeof(std::int32_t));
        if (format_ == ESamples::UNSIGNED_8) {
            auto converted = convertToUnsigned8(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::SIGNED_16_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::SIGNED_16_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::FLOAT_32_BIG_ENDIAN) {
            auto converted = convertToFloat32BE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::FLOAT_32_LITTLE_ENDIAN) {
            auto converted = convertToFloat32LE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::SIGNED_32_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::SIGNED_32_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(baseSample);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else {
            throw absl::InvalidArgumentError("sample type is not supported");
        }
    }

    for (std::size_t frame = size - trail; frame < size; ++frame) {
        if (format_ == ESamples::UNSIGNED_8) {
            auto converted = convertToUnsigned8(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::SIGNED_16_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::SIGNED_16_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::FLOAT_32_BIG_ENDIAN) {
            auto converted = convertToFloat32BE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::FLOAT_32_LITTLE_ENDIAN) {
            auto converted = convertToFloat32LE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::SIGNED_32_BIG_ENDIAN) {
            auto converted = convertToSigned32BE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else if (format_ == ESamples::SIGNED_32_LITTLE_ENDIAN) {
            auto converted = convertToSigned32LE(Silence);
            std::memcpy(dest + frame * sampleSize_, &converted, sizeof(converted));
        } else {
            throw absl::InvalidArgumentError("sample type is not supported");
        }
    }

    return absl::StatusOr<int>(size - trail);
}

absl::StatusOr<int> ReadHandle::write(const std::int32_t* src, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    if (size > buffer_->writableSize()) {
        PLOG(plog::warning) << "overrun on handle: " << this 
            << "; cutting " << size - buffer_->writableSize() << " samples on stream";
        size = buffer_->writableSize();
    }

    for (std::size_t frame = 0; frame < size; ++frame) {
        buffer_->write((char*) (src + frame), sizeof(std::int32_t));
    }

    return absl::StatusOr<int>(size);
}

ESampleType ReadHandle::getFormat() const {
    return format_;
}

bool ReadHandle::isAlive() const noexcept {
    return owner_.lock().get();
}