#pragma once

// laar
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

    using namespace NSound::NSimple;

    // This class is responsible for handling incoming  
    class IStreamHandler {
    public:

        class IReadHandle {
        public:


            class IListener {
            public:
                virtual void onFlushed(int status) = 0;
                virtual ~IListener() = default;
            };

            // async call from rt stream
            virtual void flush() = 0;
            virtual void read(char* dest, std::size_t size) = 0;
            virtual void write(std::int32_t* src, std::size_t size) = 0;
            virtual TSimpleMessage::TStreamConfiguration::TSampleSpecification::TFormat format() const = 0;
            virtual ~IReadHandle() = default;
        };

        class IWriteHandle {
        public:

            class IListener {
            public:
                virtual void onDrained(int status) = 0;
                virtual void onFlushed(int status) = 0;
                virtual ~IListener() = default;
            };

            // async call from rt stream
            virtual void drain() = 0;
            virtual void flush() = 0;
            virtual void write(char* src, std::size_t size) = 0;
            virtual void read(std::int32_t* dest, std::size_t size) = 0;
            virtual TSimpleMessage::TStreamConfiguration::TSampleSpecification::TFormat format() const = 0;
            virtual ~IWriteHandle() = default;
        };

        virtual void init() = 0;
        virtual std::shared_ptr<IReadHandle> acquireReadHandle() = 0;
        virtual std::shared_ptr<IWriteHandle> acquireWriteHandle() = 0;
        virtual ~IStreamHandler() = default;

    };

}