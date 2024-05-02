// laar
#include "common/thread-pool.hpp"
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>
#include <cstdint>
#include <exception>
#include <sounds/interfaces/i-audio-handler.hpp>

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

        static std::shared_ptr<SoundHandler> configure(
            std::shared_ptr<laar::CallbackQueue> cbQueue,
            std::shared_ptr<laar::ThreadPool> threadPool);
        SoundHandler(
            std::shared_ptr<laar::CallbackQueue> cbQueue,
            std::shared_ptr<laar::ThreadPool> threadPool,
            Private access);

        void init() override;
        virtual std::shared_ptr<IReadHandle> acquireReadHandle() override;
        virtual std::shared_ptr<IWriteHandle> acquireWriteHandle() override;

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

    private:

        static constexpr int baseSampleRate_ = 44100;
        static constexpr std::int32_t silence = INT32_MIN;

        std::shared_ptr<laar::CallbackQueue> cbQueue_;
        std::shared_ptr<laar::ThreadPool> threadPool_;

        std::vector<std::weak_ptr<IWriteHandle>> outHandles_;
        std::vector<std::weak_ptr<IReadHandle>> inHandles_;

        struct LocalData {
            std::weak_ptr<SoundHandler> object;
            std::atomic<bool> abort;
        };

        std::unique_ptr<LocalData> local_;
        
        RtAudio audio_;
        

    };

}