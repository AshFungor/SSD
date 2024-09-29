// abseil
#include <absl/status/statusor.h>
#include <absl/status/status.h>

// laar
#include <src/ssd/sound/converter.hpp>
#include <src/ssd/sound/ring-buffer.hpp>
#include <src/ssd/sound/write-handle.hpp>
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

WriteHandle::WriteHandle(
    NSound::NClient::NBase::TBaseMessage::TStreamConfiguration config, 
    std::weak_ptr<IListener> owner
) 
    : sampleSize_(getSampleSize(config.samplespec().format()))
    , config_(std::move(config)) // think of config here
    , buffer_(std::make_unique<laar::RingBuffer>(44100 * 4 * 120))
    , owner_(std::move(owner))
{}

absl::Status WriteHandle::flush() {
    std::unique_lock<std::mutex> locked(lock_);

    buffer_->drop(buffer_->readableSize());
    return absl::OkStatus();
}

void WriteHandle::abort() {
    std::unique_lock<std::mutex> locked(lock_);

    isAlive_ = false;
}

absl::Status WriteHandle::drain() {
    return absl::OkStatus();
}

absl::StatusOr<int> WriteHandle::read(std::int32_t* dest, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    if (buffer_->readableSize() / sizeof (std::int32_t) < config_.bufferconfig().prebuffingsize()) {
        PLOG(plog::debug) << "handle " << this << " is stalled, "
            << "waiting for " << config_.bufferconfig().prebuffingsize() - buffer_->readableSize() / sizeof (std::int32_t)
            << " samples (prebuffing size is " << config_.bufferconfig().prebuffingsize()
            << "; readable size is " << buffer_->readableSize() / sizeof (std::int32_t) << ")";
        return absl::DataLossError("handle is stalled, await");
    } else {
        config_.mutable_bufferconfig()->set_prebuffingsize(0);
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

    return absl::StatusOr<int>(size - trail);
}

absl::StatusOr<int> WriteHandle::write(const char* src, std::size_t size) {
    std::unique_lock<std::mutex> locked(lock_);

    std::uint8_t sample8 = 0;
    std::uint16_t sample16 = 0;
    std::uint32_t sample32 = 0;
    std::int32_t converted = 0;

    if (size > buffer_->writableSize()) {
        PLOG(plog::warning) << "overrun on handle: " << this 
            << "; cutting " << size - buffer_->writableSize() << " samples on stream";
        size = buffer_->writableSize();
    }

    PLOG(plog::debug) << "receiving bytes in handle: " << size;

    for (std::size_t frame = 0; frame < size; ++frame) {
        switch (config_.samplespec().format()) {
            case ESamples::UNSIGNED_8:
                std::memcpy(&sample8, src + frame * sizeof(sample8), sizeof(sample8));
                converted = convertFromUnsigned8(sample8);
                break;
            case ESamples::SIGNED_16_BIG_ENDIAN:
                std::memcpy(&sample16, src + frame * sizeof(sample16), sizeof(sample16));
                converted = convertFromSigned16BE(sample16);
                break;
            case ESamples::SIGNED_16_LITTLE_ENDIAN:
                std::memcpy(&sample16, src + frame * sizeof(sample16), sizeof(sample16));
                converted = convertFromSigned16LE(sample16);
                break;
            case ESamples::FLOAT_32_BIG_ENDIAN:
                std::memcpy(&sample32, src + frame * sizeof(sample32), sizeof(sample32));
                converted = convertFromFloat32BE(sample32);
                break;
            case ESamples::FLOAT_32_LITTLE_ENDIAN:
                std::memcpy(&sample32, src + frame * sizeof(sample32), sizeof(sample32));
                converted = convertFromFloat32LE(sample32);
                break;
            case ESamples::SIGNED_32_BIG_ENDIAN:
                std::memcpy(&sample32, src + frame * sizeof(sample32), sizeof(sample32));
                converted = convertFromSigned32BE(sample32);
                break;
            case ESamples::SIGNED_32_LITTLE_ENDIAN:
                std::memcpy(&sample32, src + frame * sizeof(sample32), sizeof(sample32));
                converted = convertFromSigned32LE(sample32);
                break;
            default:
                std::abort();
        }
        buffer_->write((char*) &converted, sizeof(std::uint32_t));
    }

    return absl::StatusOr<int>(size);
}

ESampleType WriteHandle::getFormat() const {
    return config_.samplespec().format();
}

bool WriteHandle::isAlive() noexcept {
    std::unique_lock<std::mutex> locked(lock_);

    return isAlive_;
}