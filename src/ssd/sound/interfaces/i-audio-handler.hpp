#pragma once

// laar
#include <sounds/converter.hpp>
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>

// RtAudio
#include <RtAudio.h>

// std
#include <memory>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// proto
#include <protos/client/simple/simple.pb.h>


namespace laar {

    inline constexpr int BaseSampleRate = 44100;
    inline constexpr std::int32_t Silence = INT32_MIN;

    namespace status {
        inline constexpr int STALLED = 3;
        inline constexpr int OVERRUN = 2;
        inline constexpr int UNDERRUN = 1;
        inline constexpr int SUCCESS = 0;
        inline constexpr int NOT_IMPLEMENTED = -1;
    }

    namespace rtcontrol {
        inline constexpr int ABORT = 2;
        inline constexpr int DRAIN = 1;
        inline constexpr int SUCCESS = 0;
    }

    using namespace NSound::NSimple;

    // This class is responsible for handling incoming  
    class IStreamHandler {
    public:

        class IHandle {
        public:

            class IListener {
            public:
                virtual void onBufferDrained(int status) = 0;
                virtual void onBufferFlushed(int status) = 0;
                virtual void onBytesRequested(std::size_t size) = 0;
                virtual ~IListener() = default;
            };

            // discard buffer
            virtual int flush() = 0;
            // io functions, int32 is used by audio stream
            // and char an arbitrary type for storing audio in any format
            virtual int read(char* dest, std::size_t size) = 0;
            virtual int write(const char* src, std::size_t size) = 0;
            virtual int read(std::int32_t* dest, std::size_t size) = 0;
            virtual int write(const std::int32_t* src, std::size_t size) = 0;
            virtual ESampleType format() const = 0;
            virtual bool alive() const noexcept = 0;

            virtual ~IHandle() = default; 
        };

        class IReadHandle : public IHandle {
        public:
            virtual ~IReadHandle() = default;
            virtual int read(char* src, std::size_t size) override = 0;
            virtual int write(const std::int32_t* dest, std::size_t size) override = 0;
            virtual bool alive() const noexcept = 0;

        private:
            virtual int write(const char* src, std::size_t size) override { return status::NOT_IMPLEMENTED; }
            virtual int read(std::int32_t* dest, std::size_t size) override { return status::NOT_IMPLEMENTED; }
        };

        class IWriteHandle : public IHandle {
        public:
            virtual ~IWriteHandle() = default;
            virtual int write(const char* src, std::size_t size) override = 0;
            virtual int read(std::int32_t* dest, std::size_t size) override = 0;
            virtual bool alive() const noexcept = 0;

        private:
            virtual int read(char* src, std::size_t size) override { return status::NOT_IMPLEMENTED; }
            virtual int write(const std::int32_t* dest, std::size_t size) override { return status::NOT_IMPLEMENTED; }
        };

        virtual void init() = 0;
        virtual std::shared_ptr<IReadHandle> acquireReadHandle(
            TSimpleMessage::TStreamConfiguration config,
            std::weak_ptr<IHandle::IListener> owner) = 0;
        virtual std::shared_ptr<IWriteHandle> acquireWriteHandle(
            TSimpleMessage::TStreamConfiguration config,
            std::weak_ptr<IHandle::IListener> owner) = 0;

        virtual ~IStreamHandler() = default;

    };

}