#pragma once

// abseil
#include <absl/status/status.h>
#include <absl/status/statusor.h>

// laar
#include <src/ssd/sound/converter.hpp>
#include <src/ssd/sound/ring-buffer.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>

// RtAudio
#include <RtAudio.h>

// std
#include <memory>

// proto
#include <protos/client/base.pb.h>

namespace laar {

    class WriteHandle : public IStreamHandler::IWriteHandle {
    public:

        using TStreamConfiguration = NSound::NCommon::TStreamConfiguration;

        WriteHandle(
            TStreamConfiguration config, 
            std::weak_ptr<IListener> owner
        );

        // IStreamHandler::IReadHandle implementation
        virtual absl::Status flush() override;
        virtual absl::Status drain() override;
        virtual void abort() override;
        // IO operations
        virtual absl::StatusOr<int> read(std::int32_t* dest, std::size_t size) override;
        virtual absl::StatusOr<int> write(const char* src, std::size_t size) override;
        // // setters
        // virtual float getVolume() const override;
        // // getters
        // virtual void setVolume(float volume) const override;
        virtual ESampleType getFormat() const override;
        // condition
        virtual bool isAlive() noexcept override;

    private:
        bool isAlive_;
        std::size_t sampleSize_;

        // buffer config
        TStreamConfiguration config_;

        std::mutex lock_;
        std::unique_ptr<laar::RingBuffer> buffer_;
        std::weak_ptr<IListener> owner_;
        
    };

}