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

    class WriteHandle : public IStreamHandler::IWriteHandle {
    public:

        WriteHandle(TSimpleMessage::TStreamConfiguration config, std::weak_ptr<IListener> owner);
        // IStreamHandler::IReadHandle implementation
        virtual int flush() override;
        virtual int read(std::int32_t* dest, std::size_t size) override;
        virtual int write(const char* src, std::size_t size) override;
        virtual ESampleType format() const override;

    private:
        TSimpleMessage::TStreamConfiguration::TBufferConfiguration bufferConfig_;

        ESampleType format_;
        std::size_t sampleSize_;

        std::mutex lock_;
        std::unique_ptr<laar::RingBuffer> buffer_;
        std::weak_ptr<IListener> owner_;
        
    };

}