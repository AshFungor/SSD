// laar
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>

// alsa
#include <alsa/asoundlib.h>
#include <alsa/control.h>
#include <alsa/error.h>
#include <alsa/conf.h>
#include <alsa/pcm.h>

// std
#include <format>
#include <memory>
#include <string>

// proto
#include <protos/client/simple/simple.pb.h>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// local
#include "audio-handler.hpp"

using namespace sound;


std::shared_ptr<SoundHandler> SoundHandler::configure(
    std::shared_ptr<laar::CallbackQueue> cbQueue,
    NSound::NSimple::TSimpleMessage::TStreamConfiguration config) 
{
    return std::make_shared<SoundHandler>(std::move(cbQueue), std::move(config), Private());    
}

SoundHandler::SoundHandler(
    std::shared_ptr<laar::CallbackQueue> cbQueue, 
    NSound::NSimple::TSimpleMessage::TStreamConfiguration config, 
    Private access)
    : cbQueue_(std::move(cbQueue))
    , clientConfig_(std::move(config))
    , pcmHandle_(nullptr, &snd_pcm_close)
    , hwParams_(nullptr, &snd_pcm_hw_params_free)
    , swParams_(nullptr, &snd_pcm_sw_params_free)
{}

void SoundHandler::init() {
    snd_pcm_t* handle = nullptr;
    int error = 0;

    if ((error = snd_pcm_open(&handle, device_.c_str(), getStreamDir(), mode_)) < 0) {
        onError("error opening device: {}", snd_strerror(error));
    }
    pcmHandle_.reset(handle);

    hwInit();
    swInit();
}

void SoundHandler::hwInit() {
    snd_pcm_hw_params_t* params = nullptr;
    snd_pcm_hw_params_alloca(&params);

    if (!pcmHandle_) {
        onError("broken init order: pcm handle is nullptr");
    }
    PLOG(plog::debug) << "handler check is passed, init() hw params";

    int error = 0;
    if ((error = snd_pcm_hw_params_any(pcmHandle_.get(), params)) < 0) {
        onError("unable to acquire params range for device: {}, error: {}", pcmHandle_.get(), snd_strerror(error));
    }

    if ((error = snd_pcm_hw_params_set_rate_resample(pcmHandle_.get(), params, true)) < 0) {
        onError("hw params: unable to set resampling, error: {}", snd_strerror(error));
    }
    if ((error = snd_pcm_hw_params_set_access(pcmHandle_.get(), params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        onError("hw params: unable to set interleaved access, error: {}", snd_strerror(error));
    }
    if ((error = snd_pcm_hw_params_set_format(pcmHandle_.get(), params, getFormat())) < 0) {
        onError("hw params: unable to set format {}, error: {}", getFormat(), snd_strerror(error));
    }
}

snd_pcm_stream_t SoundHandler::getStreamDir() {
    using namespace NSound::NSimple;

    switch (clientConfig_.direction()) {
    case TSimpleMessage_TStreamConfiguration_TStreamDirection_PLAYBACK:
        return SND_PCM_STREAM_PLAYBACK;
    case TSimpleMessage_TStreamConfiguration_TStreamDirection_UPLOAD:
        return SND_PCM_STREAM_CAPTURE;
    default:
        onError("Could not resolve stream direction, received value: " + std::to_string(clientConfig_.direction()));
    }

    return SND_PCM_STREAM_LAST;
}

snd_pcm_format_t SoundHandler::getFormat() {
    using namespace NSound::NSimple;

    switch (clientConfig_.sample_spec().format()) {
    case TSimpleMessage_TStreamConfiguration_TSampleSpecification_TFormat_UNSIGNED_8:
        return SND_PCM_FORMAT_U8;
    case TSimpleMessage_TStreamConfiguration_TSampleSpecification_TFormat_SIGNED_16_BIG_ENDIAN:
        return SND_PCM_FORMAT_S16_BE;
    case TSimpleMessage_TStreamConfiguration_TSampleSpecification_TFormat_SIGNED_16_LITTLE_ENDIAN:
        return SND_PCM_FORMAT_S16_LE;
    case TSimpleMessage_TStreamConfiguration_TSampleSpecification_TFormat_FLOAT_32_LITTLE_ENDIAN:
        return SND_PCM_FORMAT_FLOAT_LE;
    case TSimpleMessage_TStreamConfiguration_TSampleSpecification_TFormat_FLOAT_32_BIG_ENDIAN:
        return SND_PCM_FORMAT_FLOAT_BE;
    case TSimpleMessage_TStreamConfiguration_TSampleSpecification_TFormat_SIGNED_32_LITTLE_ENDIAN:
        return SND_PCM_FORMAT_S32_LE;
    case TSimpleMessage_TStreamConfiguration_TSampleSpecification_TFormat_SIGNED_32_BIG_ENDIAN:
        return SND_PCM_FORMAT_S32_BE;
    default:
        onError("sample format is not supported: ", clientConfig_.sample_spec().format());
    }

    return SND_PCM_FORMAT_UNKNOWN;
}