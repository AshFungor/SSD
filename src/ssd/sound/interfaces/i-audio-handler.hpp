#pragma once

// abseil
#include <absl/status/status.h>
#include <absl/status/statusor.h>

// laar
#include <src/ssd/sound/converter.hpp>

// std
#include <memory>

// proto
#include <protos/client/base.pb.h>


namespace laar {

    inline constexpr int BaseSampleRate = 44100;
    inline constexpr std::int32_t Silence = INT32_MIN;
    inline constexpr std::int32_t BaseSampleSize = sizeof Silence;

    namespace rtcontrol {
        inline constexpr int ABORT = 2;
        inline constexpr int DRAIN = 1;
        inline constexpr int SUCCESS = 0;
    }

    // This class is responsible for handling incoming  
    class IStreamHandler {
    public:

        class IHandle {
        public:

            class IListener {
            public:
                virtual void onBufferDrained(int status) = 0;
                virtual void onBufferFlushed(int status) = 0;
                virtual ~IListener() = default;
            };

            // discard buffer
            virtual absl::Status flush() = 0;
            virtual absl::Status drain() = 0;
            virtual void abort() = 0;

            // IO for override, take not that not all of them valid in context of children classes
            virtual absl::StatusOr<int> read(char* src, std::size_t size) = 0;
            virtual absl::StatusOr<int> read(std::int32_t* dest, std::size_t size) = 0;
            virtual absl::StatusOr<int> write(const char* src, std::size_t size) = 0;
            virtual absl::StatusOr<int> write(const std::int32_t* dest, std::size_t size) = 0;

            // // getters
            virtual ESampleType getFormat() const = 0;

            // // setters

            // status
            virtual bool isAlive() noexcept = 0;

            virtual ~IHandle() = default; 
        };

        class IReadHandle : public IHandle {
        public:
            virtual ~IReadHandle() = default;
            virtual absl::StatusOr<int> read(char* src, std::size_t size) override = 0;
            virtual absl::StatusOr<int> write(const std::int32_t* dest, std::size_t size) override = 0;

        private:
            virtual absl::StatusOr<int> write(const char* src, std::size_t size) override { 
                return absl::InternalError("not implemented");
            }
            virtual absl::StatusOr<int> read(std::int32_t* dest, std::size_t size) override { 
                return absl::InternalError("not implemented");
            }
        };

        class IWriteHandle : public IHandle {
        public:
            virtual ~IWriteHandle() = default;
            virtual absl::StatusOr<int> write(const char* src, std::size_t size) override = 0;
            virtual absl::StatusOr<int> read(std::int32_t* dest, std::size_t size) override = 0;

        private:
            virtual absl::StatusOr<int> read(char* src, std::size_t size) override { 
                return absl::InternalError("not implemented");
            }
            virtual absl::StatusOr<int> write(const std::int32_t* dest, std::size_t size) override { 
                return absl::InternalError("not implemented");
            }
        };

        virtual void init() = 0;

        // // getters
        // virtual float getVolume() const = 0;

        // // setters
        // virtual void setVolume(float volume) const = 0;

        virtual std::shared_ptr<IReadHandle> acquireReadHandle(
            NSound::NClient::NBase::TBaseMessage::TStreamConfiguration config,
            std::weak_ptr<IHandle::IListener> owner
        ) = 0;

        virtual std::shared_ptr<IWriteHandle> acquireWriteHandle(
            NSound::NClient::NBase::TBaseMessage::TStreamConfiguration config,
            std::weak_ptr<IHandle::IListener> owner
        ) = 0;

        virtual ~IStreamHandler() = default;
    };

}