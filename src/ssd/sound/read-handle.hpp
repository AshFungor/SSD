#pragma once

// laar
#include <sounds/interfaces/i-audio-handler.hpp>
#include <common/callback-queue.hpp>
#include <common/ring-buffer.hpp>
#include <common/exceptions.hpp>
#include <sounds/converter.hpp>

// RtAudio
#include <RtAudio.h>

// std
#include <memory>

// proto
#include <protos/client/simple/simple.pb.h>

namespace laar {

    class ReadHandle : public IStreamHandler::IReadHandle {
    public:

        ReadHandle(TSimpleMessage::TStreamConfiguration config, std::weak_ptr<IListener> owner);
        // IStreamHandler::IReadHandle implementation
        virtual int flush() override;
        virtual int read(char* dest, std::size_t size) override;
        virtual int write(const std::int32_t* src, std::size_t size) override;
        virtual bool alive() const noexcept override;
        virtual ESampleType format() const override;

    private:
        ESampleType format_;
        std::size_t sampleSize_;

        std::mutex lock_;
        std::unique_ptr<laar::RingBuffer> buffer_;
        std::weak_ptr<IListener> owner_;
    };

}