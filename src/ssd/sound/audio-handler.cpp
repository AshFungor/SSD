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
#include <condition_variable>
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
#include "common/macros.hpp"

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
    SSD_ENSURE_THREAD(cbQueue_);

    snd_pcm_t* handle = nullptr;
    int error = 0;

    if ((error = snd_pcm_open(&handle, device_.c_str(), getStreamDir(), mode_)) < 0) {
        onFatalError("error opening device: {}", snd_strerror(error));
    }
    pcmHandle_.reset(handle);

    hwInit();
    swInit();
}

void SoundHandler::hwInit() {
    SSD_ENSURE_THREAD(cbQueue_);

    snd_pcm_hw_params_t* params = nullptr;
    snd_pcm_hw_params_alloca(&params);

    if (!pcmHandle_) {
        onFatalError("broken init order: pcm handle is nullptr");
    }
    PLOG(plog::debug) << "handler check is passed, init() hw params";

    int error = 0;
    if ((error = snd_pcm_hw_params_any(pcmHandle_.get(), params)) < 0) {
        onFatalError("unable to acquire params range for device: {}, error: {}", (std::size_t) pcmHandle_.get(), snd_strerror(error));
    }

    if ((error = snd_pcm_hw_params_set_rate_resample(pcmHandle_.get(), params, true)) < 0) {
        onFatalError("hw params: unable to set resampling, error: {}", snd_strerror(error));
    }
    if ((error = snd_pcm_hw_params_set_access(pcmHandle_.get(), params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        onFatalError("hw params: unable to set interleaved access, error: {}", snd_strerror(error));
    }
    if ((error = snd_pcm_hw_params_set_format(pcmHandle_.get(), params, getFormat())) < 0) {
        onFatalError("hw params: unable to set format {}, error: {}", (std::size_t) getFormat(), snd_strerror(error));
    }

    if ((error = snd_pcm_hw_params(pcmHandle_.get(), params)) < 0) {
        onFatalError("hw params: unable to set params for device: {}", snd_strerror(error));
    }
    hwParams_.reset(params);
}

void SoundHandler::swInit() {
    SSD_ENSURE_THREAD(cbQueue_);

    snd_pcm_sw_params_t* params = nullptr;
    snd_pcm_sw_params_alloca(&params);

    if (!pcmHandle_ || !hwParams_) {
        onFatalError("broken init order: pcm handle and/or hw params are nullptr");
    }
    PLOG(plog::debug) << "handler check is passed, init() sw params";

    int error = 0;
    if ((error = snd_pcm_sw_params_set_avail_min(pcmHandle_.get(), params, clientConfig_.buffer_config().prebuffing_size())) < 0) {
        onFatalError("sw params: unable to set avail min for playback: {}", snd_strerror(error));
    }

    if ((error = snd_pcm_sw_params(pcmHandle_.get(), params)) < 0) {
        onFatalError("sw params: unable to set sw params: {}", snd_strerror(error));
    }
    swParams_.reset(params);
}

snd_pcm_stream_t SoundHandler::getStreamDir() {
    SSD_ENSURE_THREAD(cbQueue_);

    using namespace NSound::NSimple;

    switch (clientConfig_.direction()) {
    case TSimpleMessage::TStreamConfiguration::PLAYBACK:
        return SND_PCM_STREAM_PLAYBACK;
    case TSimpleMessage::TStreamConfiguration::RECORD:
        return SND_PCM_STREAM_CAPTURE;
    default:
        onFatalError("Could not resolve stream direction, received value: " + std::to_string(clientConfig_.direction()));
    }

    return static_cast<snd_pcm_stream_t>(-1);
}

snd_pcm_format_t SoundHandler::getFormat() {
    SSD_ENSURE_THREAD(cbQueue_);

    using namespace NSound::NSimple;

    switch (clientConfig_.sample_spec().format()) {
    case TSimpleMessage::TStreamConfiguration::TSampleSpecification::UNSIGNED_8:
        return SND_PCM_FORMAT_U8;
    case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_BIG_ENDIAN:
        return SND_PCM_FORMAT_S16_BE;
    case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_LITTLE_ENDIAN:
        return SND_PCM_FORMAT_S16_LE;
    case TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_LITTLE_ENDIAN:
        return SND_PCM_FORMAT_FLOAT_LE;
    case TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_BIG_ENDIAN:
        return SND_PCM_FORMAT_FLOAT_BE;
    case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN:
        return SND_PCM_FORMAT_S32_LE;
    case TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_BIG_ENDIAN:
        return SND_PCM_FORMAT_S32_BE;
    default:
        onFatalError("sample format is not supported: ", (std::size_t) clientConfig_.sample_spec().format());
    }

    return SND_PCM_FORMAT_UNKNOWN;
}

void SoundHandler::xrunRecovery(int error) {
    SSD_ENSURE_THREAD(cbQueue_);

    if (error == -EPIPE) {
        error = snd_pcm_prepare(pcmHandle_.get());
        if (error < 0) {
            onFatalError("cannot recovery from underrun, prepare failed: {}", snd_strerror(error));
        }
    }
    PLOG(plog::debug) << "stream " << this << " recovered from xrun";
}

void SoundHandler::push(char* buffer, std::size_t frames) {
    cbQueue_->query([this, buffer, frames]() {
        int error = 0;
        error = snd_pcm_writei(pcmHandle_.get(), buffer, frames);
        if (error < 0) {
            onRecoveribleError(error, "failed write on sound device: {}", snd_strerror(error));
        }
    }, weak_from_this());
}

void SoundHandler::pull(char* buffer, std::size_t frames) {
    cbQueue_->query([this, buffer, frames]() {
        int error = 0;
        error = snd_pcm_readi(pcmHandle_.get(), buffer, frames);
        if (error < 0) {
            onRecoveribleError(error, "failed read on sound device: {}", snd_strerror(error));
        }
    }, weak_from_this());
}

void SoundHandler::drain() {
    cbQueue_->query([this]() {
        snd_pcm_drain(pcmHandle_.get());
    }, weak_from_this());
    cbQueue_->hang();
}