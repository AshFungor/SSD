// laar
#include "sounds/converter.hpp"
#include <sounds/interfaces/i-audio-handler.hpp>
#include <common/ring-buffer.hpp>
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

namespace laar {

    class ReadHandle : public IStreamHandler::IReadHandle {
    public:

        ReadHandle(TSimpleMessage::TStreamConfiguration config);
        // IStreamHandler::IReadHandle implementation
        virtual void flush() override;
        virtual void read(char* dest, std::size_t size) override;
        virtual void write(std::int32_t* src, std::size_t size) override;
        virtual ESampleType format() const override;

    private:
        ESampleType format_;
        std::size_t sampleSize_;

        std::mutex lock_;
        std::unique_ptr<laar::RingBuffer> buffer_;
        
    };

}