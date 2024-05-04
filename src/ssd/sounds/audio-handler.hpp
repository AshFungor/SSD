#pragma once

// laar
#include <condition_variable>
#include <sounds/interfaces/i-audio-handler.hpp>
#include <common/callback-queue.hpp>
#include <util/config-loader.hpp>
#include <common/exceptions.hpp>

// RtAudio
#include <RtAudio.h>

// json
#include <nlohmann/json_fwd.hpp>

// std
#include <exception>
#include <cstdint>
#include <memory>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// proto
#include <protos/client/simple/simple.pb.h>


namespace laar {

    constexpr int BaseSampleRate = 44100;
    constexpr std::int32_t Silence = INT32_MIN;

    namespace status {

        constexpr int OVERRUN = 2;
        constexpr int UNDERRUN = 1;
        constexpr int SUCCESS = 0;

    }

    namespace rtcontrol {
        constexpr int ABORT = 2;
        constexpr int DRAIN = 1;
        constexpr int SUCCESS = 0;
    }

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

    class SoundHandler 
        : public std::enable_shared_from_this<SoundHandler> 
        , public IStreamHandler {
    private: struct Private {};
    public:

        static std::shared_ptr<SoundHandler> configure(std::shared_ptr<laar::ConfigHandler> configHandler);
        SoundHandler(std::shared_ptr<laar::ConfigHandler> configHandler, Private access);

        void init() override;
        virtual std::shared_ptr<IReadHandle> acquireReadHandle(TSimpleMessage::TStreamConfiguration config) override;
        virtual std::shared_ptr<IWriteHandle> acquireWriteHandle(TSimpleMessage::TStreamConfiguration config) override;

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

    private:
        struct LocalData;

        void onError(std::exception error);
        std::unique_ptr<LocalData> makeLocalData();

        void parseDefaultConfig(const nlohmann::json& config);

    private:

        std::condition_variable cv_;
        bool init_;

        std::vector<std::weak_ptr<IWriteHandle>> outHandles_;
        std::vector<std::weak_ptr<IReadHandle>> inHandles_;

        std::shared_ptr<laar::ConfigHandler> configHandler_;

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