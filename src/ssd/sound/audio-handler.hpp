// laar
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>

// alsa
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

// std
#include <memory>

// proto
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
        void pushSome(char* buffer, std::size_t n);
        void pullSome(char* buffer, std::size_t n);
        void drain();

        ~SoundHandler();

    private:

        template<typename... FormatArgs>
        void onError(std::string format, FormatArgs... args);

        void hwInit();
        void swInit();

        snd_pcm_stream_t getStreamDir();
        snd_pcm_format_t getFormat();

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