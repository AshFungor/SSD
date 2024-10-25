#pragma once

// abseil
#include <absl/status/status.h>

// laar
#include <src/ssd/sound/ring-buffer.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// RtAudio
#include <RtAudio.h>

// std
#include <memory>

// proto
#include <protos/client/base.pb.h>


namespace laar {

    class ReadHandle : public IStreamHandler::IReadHandle {
    public:

        ReadHandle(
            NSound::NCommon::TStreamConfiguration config, 
            std::weak_ptr<IListener> owner
        );

        // IStreamHandler::IReadHandle implementation
        // manipulation
        virtual absl::Status flush() override;
        virtual absl::Status drain() override;
        virtual void abort() override;
        // IO operations
        virtual absl::StatusOr<int> read(char* dest, std::size_t size) override;
        virtual absl::StatusOr<int> write(const std::int32_t* src, std::size_t size) override;
        // // setters
        // virtual float getVolume() const override;
        // // getters
        // virtual void setVolume(float volume) const override;
        virtual ESampleType getFormat() const override;
        // condition
        virtual bool isAlive() noexcept override;

    private:
        bool isAlive_;

        ESampleType format_;
        std::size_t sampleSize_;

        std::mutex lock_;
        std::unique_ptr<laar::RingBuffer> buffer_;
        std::weak_ptr<IListener> owner_;
    };

}