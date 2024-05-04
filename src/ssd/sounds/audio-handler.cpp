// laar
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>
#include <sounds/read-handle.hpp>
#include <sounds/write-handle.hpp>

// std
#include <climits>
#include <cstdint>
#include <memory>
#include <string>
#include <mutex>

// proto
#include <protos/client/simple/simple.pb.h>

// plog
#include <plog/Severity.h>
#include <plog/Log.h>

// RtAudio
#include <RtAudio.h>

// local
#include "audio-handler.hpp"

using namespace laar;

namespace {

    constexpr auto SOUND_SECTION = "sound";

}

std::shared_ptr<SoundHandler> SoundHandler::configure(std::shared_ptr<laar::ConfigHandler> configHandler)
{
    return std::make_shared<SoundHandler>(std::move(configHandler), Private());    
}

SoundHandler::SoundHandler(
    std::shared_ptr<laar::ConfigHandler> configHandler,
    Private access)
: audio_(RtAudio::Api::LINUX_ALSA)
, configHandler_(std::move(configHandler))
, local_(makeLocalData())
, init_(false)
{}

void SoundHandler::init() {

    configHandler_->subscribeOnDefaultConfig(
        SOUND_SECTION, 
        [&](const auto& config) {
            parseDefaultConfig(config);
        }, 
        weak_from_this());

    if (init_) {
        throw laar::LaarBadInit();
    }

    std::unique_lock<std::mutex> locked(local_->handlerLock);
    cv_.wait(locked, [&]{
        return init_;
    });

    auto devices = audio_.getDeviceIds();
    for (auto& id : devices) {
        auto info = audio_.getDeviceInfo(id);
        PLOG(plog::debug) << "Available device with id: " << id << ", name: " << info.name;
    }

    auto inputDevice = audio_.getDefaultInputDevice();
    auto outputDevice = audio_.getDefaultOutputDevice();

    if (!(inputDevice && outputDevice)) {
        onError(laar::LaarSoundHandlerError("Device for IO was not acquired"));
    }

    if (settings_.isCaptureEnabled) {
        RtAudio::StreamParameters inputParams;
        inputParams.deviceId = inputDevice;
        inputParams.firstChannel = 0;
        inputParams.nChannels = 1;
        unsigned int bufferFrames = UINT_MAX;
        auto inputStreamError = audio_.openStream(
            nullptr, 
            &inputParams, 
            RTAUDIO_SINT32, 
            BaseSampleRate, 
            &bufferFrames, 
            &readCallback, 
            (void*)local_.get());

        if (inputStreamError) {
            onError(laar::LaarSoundHandlerError(audio_.getErrorText()));
        }

        PLOG(plog::info) << "input stream opened with nframes buffer: " << bufferFrames;
    } else {
        PLOG(plog::warning) << "capture was not open";
    }

    if (settings_.isPlaybackEnabled) {
        RtAudio::StreamParameters outParams;
        outParams.deviceId = outputDevice;
        outParams.firstChannel = 0;
        outParams.nChannels = 1;
        unsigned int bufferFrames = UINT_MAX;
        auto outStreamError = audio_.openStream(
            &outParams, 
            nullptr, 
            RTAUDIO_SINT32, 
            BaseSampleRate, 
            &bufferFrames, 
            &writeCallback, 
            (void*)local_.get());

        if (outStreamError) {
            onError(laar::LaarSoundHandlerError(audio_.getErrorText()));
        }

        PLOG(plog::info) << "output stream opened with nframes buffer: " << bufferFrames;
    } else {
        PLOG(plog::warning) << "playback was not open";
    }
}

std::unique_ptr<SoundHandler::LocalData> SoundHandler::makeLocalData() {
    auto data = std::make_unique<LocalData>();
    data->object = shared_from_this();
    data->abort = false;
    return data;
}

void SoundHandler::onError(std::exception error) {
    PLOG(plog::error) << "error in sound module: " << error.what();
    throw error;
}

int laar::writeCallback(
    void* out, 
    void* in, 
    unsigned int frames, 
    double streamTime, 
    RtAudioStreamStatus status,
    void* local) 
{
    auto data = (SoundHandler::LocalData*) local;
    auto handler = data->object.lock();

    if (!handler) {
        // abort stream
        PLOG(plog::warning) << "handler is nullptr, exiting on playback stream";
        return rtcontrol::ABORT;
    }

    if (data->abort) {
        PLOG(plog::warning) << "draining and aborting playback stream";
        // drain stream and close it
        return rtcontrol::DRAIN;
    }

    std::vector<std::unique_ptr<std::int32_t[]>> buffers;
    {
        std::unique_lock<std::mutex> locked(data->handlerLock);
        for (auto& handle : handler->outHandles_) {
            if (auto lockedHandle = handle.lock()) {
                buffers.emplace_back(std::make_unique<std::int32_t[]>(frames));
                lockedHandle->read(buffers.back().get(), frames);
            }
        }
    }

    auto result = (std::int32_t*) out;

    for (std::size_t frame = 0; frame < frames; ++frame) {
        bool isFirst = true;
        for (auto& buffer : buffers) {
            if (isFirst) {
                isFirst = false;
                result[frame] = buffer[frame];
            } else {
                result[frame] = 2 * ((std::int64_t) buffer[frame] + result[frame]) 
                    - (std::int64_t) buffer[frame] * result[frame] / (INT32_MAX / 2) - INT32_MAX;
            }
        }

        if (buffers.empty()) {
            // put silence
            result[frame] = Silence;
        }
    }

    return rtcontrol::SUCCESS;
}

int laar::readCallback(
    void* out, 
    void* in, 
    unsigned int frames, 
    double streamTime, 
    RtAudioStreamStatus status,
    void* local) 
{
    auto data = (SoundHandler::LocalData*) local;
    auto handler = data->object.lock();

    if (!handler) {
        // abort stream
        return rtcontrol::ABORT;
    }

    if (data->abort) {
        // drain stream and close it
        return rtcontrol::DRAIN;
    }

    auto result = (std::int32_t*) in;
    {
        std::unique_lock<std::mutex> locked(data->handlerLock);
        for (auto& handle : handler->inHandles_) {
            if (auto lockedHandle = handle.lock()) {
                lockedHandle->write(result, frames);
            }
        }
    }

    return rtcontrol::SUCCESS;
}

void SoundHandler::parseDefaultConfig(const nlohmann::json& config) {

    settings_.isCaptureEnabled = config.value<bool>("isCaptureEnabled", true);
    settings_.isPlaybackEnabled = config.value<bool>("isPlaybackEnabled", true);

    init_ = true;
    cv_.notify_one();
}

std::shared_ptr<SoundHandler::IReadHandle> SoundHandler::acquireReadHandle(TSimpleMessage::TStreamConfiguration config) {
    std::unique_lock<std::mutex> locked(local_->handlerLock);
    auto handle = std::make_shared<laar::ReadHandle>(std::move(config));
    inHandles_.push_back(handle);
    return handle;
}

std::shared_ptr<SoundHandler::IWriteHandle> SoundHandler::acquireWriteHandle(TSimpleMessage::TStreamConfiguration config) {
    std::unique_lock<std::mutex> locked(local_->handlerLock);
    auto handle = std::make_shared<laar::WriteHandle>(std::move(config));
    outHandles_.push_back(handle);
    return handle;
}