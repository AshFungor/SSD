#pragma once

// laar
#include <src/common/thread-pool.hpp>
#include <src/common/exceptions.hpp>
#include <src/common/callback-queue.hpp>
#include <src/ssd/sound/dispatchers/tube-dispatcher.hpp>
#include <src/ssd/sound/jobs/async-dispatching-job.hpp>
#include <src/ssd/sound/dispatchers/bass-router-dispatcher.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>
#include <src/ssd/util/config-loader.hpp>

// RtAudio
#include <RtAudio.h>

// json
#include <nlohmann/json_fwd.hpp>

// std
#include <condition_variable>
#include <exception>
#include <cstdint>
#include <memory>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// proto
#include <protos/client/simple/simple.pb.h>


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

    class SoundHandler 
        : public std::enable_shared_from_this<SoundHandler> 
        , public laar::IStreamHandler {
    private: struct Private {};
    public:

        static std::shared_ptr<SoundHandler> configure(
            std::shared_ptr<laar::ConfigHandler> configHandler,
            std::shared_ptr<laar::ThreadPool> pool
        );

        SoundHandler(
            std::shared_ptr<laar::ConfigHandler> configHandler, 
            std::shared_ptr<laar::ThreadPool> pool,
            Private access
        );

        void init() override;
        virtual std::shared_ptr<IReadHandle> acquireReadHandle(
            TSimpleMessage::TStreamConfiguration config,
            std::weak_ptr<IStreamHandler::IHandle::IListener> owner) override;
        virtual std::shared_ptr<IWriteHandle> acquireWriteHandle(
            TSimpleMessage::TStreamConfiguration config,
            std::weak_ptr<IStreamHandler::IHandle::IListener> owner) override;

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
        std::unique_ptr<std::int32_t[]> squash(SoundHandler::LocalData* data, std::size_t frames);

    private:

        std::shared_ptr<laar::ThreadPool> pool_;

        std::condition_variable cv_;
        bool init_;

        std::shared_ptr<BassRouterDispatcher> bassDispatcher_;
        std::shared_ptr<TubeDispatcher> dispatcherManyToOne_;

        std::vector<std::weak_ptr<IWriteHandle>> outHandles_;
        std::vector<std::weak_ptr<IReadHandle>> inHandles_;

        std::shared_ptr<laar::ConfigHandler> configHandler_;

        struct LocalData {
            std::weak_ptr<SoundHandler> object;
            std::atomic<bool> abort;
            std::mutex handlerLock;

            std::unique_ptr<laar::AsyncDispatchingJob> job;
        };

        std::vector<std::unique_ptr<laar::AsyncDispatchingJob>> jobs_;

        struct Settings {
            bool isPlaybackEnabled;
            bool isCaptureEnabled;
        } settings_;

        std::unique_ptr<LocalData> local_;
        
        RtAudio audio_;
    };

}