#pragma once

// Boost
#include <boost/asio.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/io_context.hpp>

// Abseil
#include <absl/status/status.h>

// laar
#include <src/ssd/util/config-loader.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>
#include <src/ssd/sound/dispatchers/tube-dispatcher.hpp>
#include <src/ssd/sound/dispatchers/bass-router-dispatcher.hpp>

// RtAudio
#include <RtAudio.h>

// json
#include <nlohmann/json_fwd.hpp>

// std
#include <mutex>
#include <memory>
#include <cstdint>
#include <exception>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// proto
#include <protos/client/stream.pb.h>


namespace laar {

    int writeCallback(
        void* out, 
        void* in, 
        unsigned int frames, 
        double streamTime, 
        RtAudioStreamStatus status,
        void* local
    );

    int readCallback(
        void* out, 
        void* in, 
        unsigned int frames, 
        double streamTime, 
        RtAudioStreamStatus status,
        void* local
    );

    int duplexCallback(
        void* out, 
        void* in, 
        unsigned int frames, 
        double streamTime, 
        RtAudioStreamStatus status,
        void* local
    );

    class SoundHandler 
        : public std::enable_shared_from_this<SoundHandler> 
        , public laar::IStreamHandler {
    private: struct Private {};
    public:

        static std::shared_ptr<SoundHandler> configure(
            std::shared_ptr<laar::ConfigHandler> configHandler,
            std::shared_ptr<boost::asio::io_context> context
        );

        SoundHandler(
            std::shared_ptr<laar::ConfigHandler> configHandler, 
            std::shared_ptr<boost::asio::io_context> context,
            Private access
        );

        void init() override;
        virtual std::shared_ptr<IReadHandle> acquireReadHandle(
            NSound::NCommon::TStreamConfiguration config,
            std::weak_ptr<IStreamHandler::IHandle::IListener> owner
        ) override;
        virtual std::shared_ptr<IWriteHandle> acquireWriteHandle(
            NSound::NCommon::TStreamConfiguration config,
            std::weak_ptr<IStreamHandler::IHandle::IListener> owner
        ) override;

        friend int laar::writeCallback(
            void* out, 
            void* in, 
            unsigned int frames, 
            double streamTime, 
            RtAudioStreamStatus status,
            void* local
        );

        friend int laar::readCallback(
            void* out, 
            void* in, 
            unsigned int frames, 
            double streamTime, 
            RtAudioStreamStatus status,
            void* local
        );

        friend int laar::duplexCallback(
            void* out, 
            void* in, 
            unsigned int frames, 
            double streamTime, 
            RtAudioStreamStatus status,
            void* local
        );

    private:
        struct LocalData;

        void onError(std::exception error);
        std::unique_ptr<LocalData> makeLocalData();

        unsigned int probeDevices(bool isInput) noexcept;

        // builder stages
        bool checkSampleRate(const RtAudio::DeviceInfo& info, std::string& verdict) noexcept;
        bool checkName(const RtAudio::DeviceInfo& info, std::string& verdict) noexcept;
        bool checkSampleFormat(const RtAudio::DeviceInfo& info, std::string& verdict) noexcept;

        absl::Status openDuplexStream(unsigned int devInput, unsigned int devOutput);
        absl::Status openPlayback(unsigned int dev);
        absl::Status openCapture(unsigned int dev);

        void parseDefaultConfig(const nlohmann::json& config);

        std::unique_ptr<std::int32_t[]> dispatchAsync(std::unique_ptr<std::int32_t[]> in, std::size_t samples);
        std::unique_ptr<std::int32_t[]> squash(std::size_t frames);
        absl::Status unfetter(std::int32_t* source, std::size_t frames);

    private:

        std::future<std::unique_ptr<std::int32_t[]>> future_;

        std::once_flag init_;
        std::shared_ptr<boost::asio::io_context> context_;

        std::vector<std::weak_ptr<IWriteHandle>> outHandles_;
        std::vector<std::weak_ptr<IReadHandle>> inHandles_;

        std::shared_ptr<laar::ConfigHandler> configHandler_;
        std::shared_ptr<BassRouterDispatcher> bassDispatcher_;

        struct LocalData {
            std::weak_ptr<SoundHandler> object;
            std::atomic<bool> abort;
            std::mutex handlerLock;
        };

        struct Settings {
            bool isPlaybackEnabled;
            bool isCaptureEnabled;
        } settings_;

        std::unique_ptr<LocalData> local_;
        
        RtAudio audio_;
    };

}