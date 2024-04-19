// laar
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>

// alsa
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

// std
#include <format>
#include <memory>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// proto
#include <plog/Severity.h>
#include <protos/client/simple/simple.pb.h>


namespace sound {

    class SoundHandler : public std::enable_shared_from_this<SoundHandler> {
    private: struct Private {};
    public:

        static std::shared_ptr<SoundHandler> configure(
            std::shared_ptr<laar::CallbackQueue> cbQueue,
            NSound::NSimple::TSimpleMessage::TStreamConfiguration config
        );

        SoundHandler(
            std::shared_ptr<laar::CallbackQueue> cbQueue, 
            NSound::NSimple::TSimpleMessage::TStreamConfiguration config, 
            Private access
        );

        void init();
        void push(char* buffer, std::size_t frames);
        void pull(char* buffer, std::size_t frames);
        void drain();

        ~SoundHandler();

    private:

        template<typename... FormatArgs>
        void onFatalError(std::string format, FormatArgs... args);

        template<typename... FormatArgs>
        void onRecoveribleError(int error, std::string format, FormatArgs... args);

        void hwInit();
        void swInit();

        snd_pcm_stream_t getStreamDir();
        snd_pcm_format_t getFormat();

        void xrunRecovery(int error);

    private:

        // generic hw interface
        const std::string device_ = "plughw:0,0";
        // blocking mode
        const int mode_ = 0;

        std::shared_ptr<laar::CallbackQueue> cbQueue_;
        NSound::NSimple::TSimpleMessage::TStreamConfiguration clientConfig_;

        std::unique_ptr<snd_pcm_hw_params_t, void(*)(snd_pcm_hw_params_t*)> hwParams_;
        std::unique_ptr<snd_pcm_sw_params_t, void(*)(snd_pcm_sw_params_t*)> swParams_;
        std::unique_ptr<snd_pcm_t, int(*)(snd_pcm_t*)> pcmHandle_;

    };

}

template<typename... FormatArgs>
void sound::SoundHandler::onFatalError(std::string format, FormatArgs... args) {
    throw laar::LaarSoundHandlerError(std::vformat(format, std::make_format_args(args...)));
}

template<typename... FormatArgs>
void sound::SoundHandler::onRecoveribleError(int error, std::string format, FormatArgs... args) {
    PLOG(plog::warning) << std::vformat(format, std::make_format_args(args...));
    xrunRecovery(error);
}