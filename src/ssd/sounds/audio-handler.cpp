// laar
#include <climits>
#include <common/callback-queue.hpp>
#include <common/exceptions.hpp>
#include <common/macros.hpp>

// std
#include <condition_variable>
#include <cstdint>
#include <format>
#include <memory>
#include <string>

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

std::shared_ptr<SoundHandler> SoundHandler::configure(
    std::shared_ptr<laar::CallbackQueue> cbQueue,
    std::shared_ptr<laar::ThreadPool> threadPool)  
{
    return std::make_shared<SoundHandler>(std::move(cbQueue), std::move(threadPool), Private());    
}

SoundHandler::SoundHandler(
    std::shared_ptr<laar::CallbackQueue> cbQueue,
    std::shared_ptr<laar::ThreadPool> threadPool,
    Private access)
: cbQueue_(std::move(cbQueue))
, threadPool_(std::move(threadPool))
, audio_(RtAudio::Api::LINUX_ALSA)
{}

void SoundHandler::init() {
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

    RtAudio::StreamParameters inputParams;
    inputParams.deviceId = inputDevice;
    inputParams.firstChannel = 0;
    inputParams.nChannels = 1;
    unsigned int bufferFrames = UINT_MAX;
    auto inputStreamError = audio_.openStream(
        nullptr, 
        &inputParams, 
        RTAUDIO_SINT32, 
        baseSampleRate_, 
        &bufferFrames, 
        &readCallback, 
        (void*)local_.get());

    if (inputStreamError) {
        onError(laar::LaarSoundHandlerError(audio_.getErrorText()));
    }

    PLOG(plog::info) << "input stream opened with nframes buffer: " << bufferFrames;

    RtAudio::StreamParameters outParams;
    outParams.deviceId = outputDevice;
    outParams.firstChannel = 0;
    outParams.nChannels = 1;
    bufferFrames = UINT_MAX;
    auto outStreamError = audio_.openStream(
        &outParams, 
        nullptr, 
        RTAUDIO_SINT32, 
        baseSampleRate_, 
        &bufferFrames, 
        &writeCallback, 
        (void*)local_.get());

    if (outStreamError) {
        onError(laar::LaarSoundHandlerError(audio_.getErrorText()));
    }

    PLOG(plog::info) << "output stream opened with nframes buffer: " << bufferFrames;
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
        return 2;
    }

    if (data->abort) {
        // drain stream and close it
        return 1;
    }

    handler->cbQueue_->query([&]() {

        std::vector<std::unique_ptr<std::int32_t[]>> buffers;

        for (auto& handle : handler->outHandles_) {
            if (auto lockedHandle = handle.lock()) {
                buffers.emplace_back(std::make_unique<std::int32_t[]>(frames));
                lockedHandle->read(buffers.back().get(), frames);
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
                result[frame] = laar::SoundHandler::silence;
            }
        }

    });

    return 0;
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
        return 2;
    }

    if (data->abort) {
        // drain stream and close it
        return 1;
    }

    handler->cbQueue_->query([&]() {
        auto result = (std::int32_t*) in;

        for (auto& handle : handler->inHandles_) {
            if (auto lockedHandle = handle.lock()) {
                lockedHandle->write(result, frames);
            }
        }
    });

    return 0;
}

std::shared_ptr<SoundHandler::IReadHandle> SoundHandler::acquireReadHandle() {
    
}