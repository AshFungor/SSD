// laar
#include <src/common/macros.hpp>
#include <src/common/exceptions.hpp>
#include <src/common/thread-pool.hpp>
#include <src/common/callback-queue.hpp>
#include <src/ssd/sound/read-handle.hpp>
#include <src/ssd/sound/write-handle.hpp>
#include <src/ssd/util/config-loader.hpp>
#include <src/ssd/sound/audio-handler.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>
#include <src/ssd/sound/jobs/async-dispatching-job.hpp>
#include <src/ssd/sound/dispatchers/tube-dispatcher.hpp>
#include <src/ssd/sound/dispatchers/bass-router-dispatcher.hpp>

// abseil
#include <absl/status/status.h>
#include <absl/strings/match.h>
#include <absl/status/statusor.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>

// RtAudio
#include <RtAudio.h>

// json
#include <nlohmann/json_fwd.hpp>

// std
#include <cctype>
#include <memory>
#include <cstdint>
#include <exception>
#include <algorithm>
#include <condition_variable>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// proto
#include <protos/client/base.pb.h>
#include <vector>

using namespace laar;

using ESamples = 
    NSound::NClient::NBase::TBaseMessage::TStreamConfiguration::TSampleSpecification;


namespace {

    constexpr auto SOUND_SECTION = "sound";
    constexpr int inChannelShift = 0;
    constexpr int outChannelShift = 0;
    constexpr int inChannelsCount = 1;
    constexpr int outChannelsCount = 2;
    constexpr int bufferFrames = 1000;

}

std::shared_ptr<SoundHandler> SoundHandler::configure(
    std::shared_ptr<laar::ConfigHandler> configHandler,
    std::shared_ptr<laar::CallbackQueue> cbQueue
) {
    return std::make_shared<SoundHandler>(std::move(configHandler), std::move(cbQueue), Private());    
}

SoundHandler::SoundHandler(
    std::shared_ptr<laar::ConfigHandler> configHandler,
    std::shared_ptr<laar::CallbackQueue> cbQueue,
    Private access
)
    : cbQueue_(std::move(cbQueue))
    , init_(false)
    , bassDispatcher_(laar::BassRouterDispatcher::create(
        ESamplesOrder::NONINTERLEAVED, 
        ESamples::SIGNED_32_LITTLE_ENDIAN,
        BaseSampleRate,
        BassRouterDispatcher::BassRange(20, 250),
        BassRouterDispatcher::ChannelInfo(0, 1))
    )
    , configHandler_(std::move(configHandler))
    , local_(nullptr)
    , audio_(RtAudio::Api::LINUX_ALSA)
{}

void SoundHandler::init() {
    local_ = makeLocalData();

    configHandler_->subscribeOnDefaultConfig(
        SOUND_SECTION, 
        [&](const auto& config) {
            parseDefaultConfig(config);
        }, 
        weak_from_this()
    );

    std::unique_lock<std::mutex> locked(local_->handlerLock);
    cv_.wait(locked, [&]{
        return init_;
    });

    if (init_) {
        throw laar::LaarBadInit();
    }

    auto inputDevice = probeDevices(true);
    auto outputDevice = probeDevices(false);

    if (!(inputDevice && outputDevice)) {
        onError(laar::LaarSoundHandlerError("Device for IO was not acquired"));
    }

    if (settings_.isCaptureEnabled && settings_.isPlaybackEnabled) {
        PLOG(plog::info) << "opening duplex stream";
        if (absl::Status status = openDuplexStream(inputDevice, outputDevice); !status.ok()) {
            onError(laar::LaarSoundHandlerError(status.message().data()));
        }
    } else if (settings_.isCaptureEnabled) {
        PLOG(plog::info) << "opening capture stream";
        if (absl::Status status = openCapture(inputDevice); !status.ok()) {
            onError(laar::LaarSoundHandlerError(status.message().data()));
        }
    } else if (settings_.isPlaybackEnabled) {
        PLOG(plog::info) << "opening playback stream";
        if (absl::Status status = openPlayback(outputDevice); !status.ok()) {
            onError(laar::LaarSoundHandlerError(status.message().data()));
        }
    } else {
        onError(laar::LaarSoundHandlerError("at least one option for stream type must be enabled"));
    }
}

absl::Status SoundHandler::openDuplexStream(unsigned int devInput, unsigned int devOutput) {
    RtAudio::StreamParameters inParams {
        .deviceId = devInput, .nChannels = inChannelsCount, .firstChannel = inChannelShift
    };
    RtAudio::StreamParameters outParams {
        .deviceId = devOutput, .nChannels = outChannelsCount, .firstChannel = outChannelShift
    };

    unsigned int bufferFrames = ::bufferFrames;
    RtAudio::StreamOptions options;
    options.flags = RTAUDIO_MINIMIZE_LATENCY & RTAUDIO_NONINTERLEAVED;

    if (RtAudioErrorType error = 
        audio_.openStream(
            &outParams, 
            &inParams, 
            RTAUDIO_SINT32, 
            BaseSampleRate, 
            &bufferFrames, 
            &laar::duplexCallback,
            (void*)local_.get(),
            &options
        ); error
    ) {
        return absl::InternalError(audio_.getErrorText());
    }

    audio_.startStream();
    PLOG(plog::info) 
        << "duplex stream opened with nframes buffer: " << bufferFrames 
        << " with device ids: out: " << devOutput << " and in: " << devInput;

    return absl::OkStatus(); 
}

absl::Status SoundHandler::openPlayback(unsigned int dev) {
    RtAudio::StreamParameters outParams {
        .deviceId = dev, .nChannels = outChannelsCount, .firstChannel = outChannelShift
    };

    unsigned int bufferFrames = ::bufferFrames;
    RtAudio::StreamOptions options;
    options.flags = RTAUDIO_MINIMIZE_LATENCY & RTAUDIO_NONINTERLEAVED;

    if (RtAudioErrorType error = 
        audio_.openStream(
            &outParams, 
            nullptr, 
            RTAUDIO_SINT32, 
            BaseSampleRate, 
            &bufferFrames, 
            &laar::writeCallback,
            (void*)local_.get(),
            &options
        ); error
    ) {
        return absl::InternalError(audio_.getErrorText());
    }

    audio_.startStream();
    PLOG(plog::info) 
        << "output stream opened with nframes buffer: " << bufferFrames 
        << " with device id: " << dev;

    return absl::OkStatus(); 
}

absl::Status SoundHandler::openCapture(unsigned int dev) {
    RtAudio::StreamParameters inParams {
        .deviceId = dev, .nChannels = inChannelsCount, .firstChannel = inChannelShift
    };

    unsigned int bufferFrames = ::bufferFrames;
    RtAudio::StreamOptions options;
    options.flags = RTAUDIO_MINIMIZE_LATENCY & RTAUDIO_NONINTERLEAVED;

    if (RtAudioErrorType error = 
        audio_.openStream(
            nullptr, 
            &inParams, 
            RTAUDIO_SINT32, 
            BaseSampleRate, 
            &bufferFrames, 
            &laar::readCallback,
            (void*)local_.get(),
            &options
        ); error
    ) {
        return absl::InternalError(audio_.getErrorText());
    }

    audio_.startStream();
    PLOG(plog::info) 
        << "input stream opened with nframes buffer: " << bufferFrames 
        << " with device id: " << dev;

    return absl::OkStatus();
}

unsigned int SoundHandler::probeDevices(bool isInput) noexcept {
    for (auto& device : audio_.getDeviceIds()) {
        RtAudio::DeviceInfo info = audio_.getDeviceInfo(device);
        std::string verdict = absl::StrFormat("Probing device with id %d and name %s; ", device, info.name);

        bool status = true;
        // required to pass
        status &= checkSampleRate(info, verdict);
        status &= checkSampleFormat(info, verdict);
        
        if (status) {
            if (checkName(info, verdict)) {
                PLOG(plog::info) << verdict << "; pulseaudio is preferred.";
                return device;
            } else if (info.isDefaultInput && isInput) {
                PLOG(plog::info) << verdict << "; device is default input.";
                return device;
            } else if (info.isDefaultOutput && !isInput) {
                PLOG(plog::info) << verdict << "; device is default output.";
                return device;
            } else {
                PLOG(plog::info) << verdict << "; device is not preferred.";
            }
        } else {
            PLOG(plog::info) << verdict << "; device lacks critical attributes.";
        }
    }

    if (isInput) {
        return audio_.getDefaultInputDevice();
    }
    return audio_.getDefaultOutputDevice();
}

bool SoundHandler::checkSampleRate(const RtAudio::DeviceInfo& info, std::string& verdict) noexcept {
    if (
        auto iter = std::find(info.sampleRates.begin(), info.sampleRates.end(), laar::BaseSampleRate); 
        iter == info.sampleRates.end()
    ) {
        std::string available;
        for (int i = 0; i < info.sampleRates.size(); ++i) {
            if (i == info.sampleRates.size() - 1) {
                available = absl::StrCat(available, absl::StrFormat("%d; ", info.sampleRates[i]));
            } else {
                available = absl::StrCat(available, absl::StrFormat("%d, ", info.sampleRates[i]));
            }
        }

        verdict = absl::StrCat(
            verdict, 
            absl::StrFormat(
                "device does not have %d sample rate available, only the following are supported: %s",
                laar::BaseSampleRate,
                available
            )
        );

        return false;
    }

    verdict = absl::StrCat(verdict, absl::StrFormat("device supports requested sample rate: %d; ", laar::BaseSampleRate));
    return true;
}

bool SoundHandler::checkSampleFormat(const RtAudio::DeviceInfo& info, std::string& verdict) noexcept {
    verdict = absl::StrCat(verdict, "native sample rates: ");
    for (const RtAudioFormat& format : {
        RTAUDIO_SINT8, 
        RTAUDIO_SINT16,
        RTAUDIO_SINT32,
        RTAUDIO_SINT24,
        RTAUDIO_FLOAT32,
        RTAUDIO_FLOAT64
    }) {
        if (format & info.nativeFormats) {
            verdict = absl::StrCat(verdict, absl::StrFormat("%d, ", format));
        }
    }

    if (RTAUDIO_SINT32 & info.nativeFormats) {
        verdict = absl::StrCat(verdict, absl::StrFormat("required format is native %d; ", RTAUDIO_SINT32));
    } else {
        verdict = absl::StrCat(verdict, absl::StrFormat("required format is non-native %d; ", RTAUDIO_SINT32));
        // return false; ?
    }

    return true;
}

bool SoundHandler::checkName(const RtAudio::DeviceInfo& info, std::string& verdict) noexcept {
    std::string name = info.name;
    std::for_each(name.begin(), name.end(), [](char& ch) {
        ch = std::tolower(ch);
    });

    // prefer pulseaudio sink
    return absl::StrContains(name, "pulseaudio");
}

std::unique_ptr<SoundHandler::LocalData> SoundHandler::makeLocalData() {
    auto data = std::make_unique<LocalData>();
    data->object = shared_from_this();
    data->abort = false;
    data->job = nullptr;

    return data;
}

void SoundHandler::onError(std::exception error) {
    PLOG(plog::error) << "error in sound module: " << error.what();
    throw error;
}

int duplexCallback(
    void* out, 
    void* in, 
    unsigned int frames, 
    double streamTime, 
    RtAudioStreamStatus status,
    void* local)
{
    int code = laar::writeCallback(out, in, frames, streamTime, status, local);
    if (code != rtcontrol::SUCCESS) {
        return code;
    }
    return readCallback(out, in, frames, streamTime, status, local);
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

    auto result = (std::int32_t*) out;

    std::unique_ptr<int32_t[]> buffer;

    if (!data->job) {
        // pass data to avoid blocking
        buffer = handler->squash(data, frames);
    } else {
        // extract data
        buffer = data->job->result();
        if (!data->job->ready()) {
            handler->jobs_.push_back(std::move(data->job));
        }
    }

    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t sample = 0; sample < frames; ++sample) {
            result[channel * frames + sample] = buffer[sample];
        }
    }

    if (handler->jobs_.size()) {
        std::erase_if(handler->jobs_, [](auto& job) {
            return job->ready();
        });
        data->job = nullptr;
        
        return rtcontrol::SUCCESS;
    }

    auto squashed = handler->squash(data, frames);
    data->job = std::make_unique<laar::AsyncDispatchingJob>(
        handler->cbQueue_,
        handler->bassDispatcher_,
        frames,
        std::move(squashed)
    );

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
        std::vector<std::shared_ptr<IStreamHandler::IReadHandle>> expired;
        std::unique_lock<std::mutex> locked(data->handlerLock);
        for (auto& handle : handler->inHandles_) {
            if (auto lockedHandle = handle.lock()) {
                if (!lockedHandle->isAlive()) {
                    expired.push_back(lockedHandle);
                    continue;
                }

                if (absl::StatusOr<int> bytes = lockedHandle->write(result, frames); !bytes.ok() || bytes.value() < frames) {
                    if (!bytes.ok()) {
                        PLOG(plog::warning) 
                            << "Read Callback: encountered error while writing bytes to handle: "
                            << lockedHandle.get() << ", message: " << bytes.status().message();
                    } else {
                        PLOG(plog::warning)
                            << "Read Callback: only " << bytes.value() << " bytes were written to handle; expected " << frames;
                    }
                    if (absl::Status status = lockedHandle->flush(); !status.ok()) {
                        PLOG(plog::error) << "Read Callback: failed to recover handle, aborting";
                        SSD_ABORT_UNLESS(false);
                    }
                }
            }

            for (auto& handle : expired) {
                auto iter = std::find_if(handler->inHandles_.begin(), handler->inHandles_.end(), [&handle](auto& currentHandle) {
                    if (auto locked = currentHandle.lock()) {
                        return locked == handle;
                    }
                    // this case should not be possible
                    SSD_ABORT_UNLESS(false);
                });

                std::iter_swap(iter, std::next(handler->inHandles_.end(), -1));
                handler->inHandles_.pop_back();
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

std::shared_ptr<SoundHandler::IReadHandle> SoundHandler::acquireReadHandle(
    NSound::NClient::NBase::TBaseMessage::TStreamConfiguration config,
    std::weak_ptr<IStreamHandler::IHandle::IListener> owner) 
{
    std::unique_lock<std::mutex> locked(local_->handlerLock);
    auto handle = std::make_shared<laar::ReadHandle>(std::move(config), std::move(owner));
    inHandles_.push_back(handle);
    return handle;
}

std::shared_ptr<SoundHandler::IWriteHandle> SoundHandler::acquireWriteHandle(
    NSound::NClient::NBase::TBaseMessage::TStreamConfiguration config,
    std::weak_ptr<IStreamHandler::IHandle::IListener> owner) 
{
    std::unique_lock<std::mutex> locked(local_->handlerLock);
    auto handle = std::make_shared<laar::WriteHandle>(std::move(config), std::move(owner));
    outHandles_.push_back(handle);
    return handle;
}

std::unique_ptr<std::int32_t[]> SoundHandler::squash(SoundHandler::LocalData* data, std::size_t frames) {
    std::vector<std::unique_ptr<std::int32_t[]>> buffers;
    {
        std::unique_lock<std::mutex> locked(data->handlerLock);

        std::vector<std::shared_ptr<laar::IStreamHandler::IWriteHandle>> expired;
        for (auto& handle : outHandles_) {
            if (auto lockedHandle = handle.lock()) {
                if (!lockedHandle->isAlive()) {
                    expired.push_back(lockedHandle);
                }

                buffers.emplace_back(std::make_unique<std::int32_t[]>(frames));
                if (absl::StatusOr<int> bytes = lockedHandle->read(buffers.back().get(), frames); !bytes.ok()) {
                    PLOG(plog::warning) 
                        << "Write Callback: failed to get " << frames 
                        << " bytes from handle: " << lockedHandle.get() 
                        << "; error: " << bytes.status().message();
                }
            }
        }

        for (auto& handle : expired) {
            auto iter = std::find_if(outHandles_.begin(), outHandles_.end(), [&handle](auto& currentHandle) {
                if (auto locked = currentHandle.lock()) {
                    return locked == handle;
                }
                // this case should not be possible
                SSD_ABORT_UNLESS(false);
            });

            std::iter_swap(iter, std::next(outHandles_.end(), -1));
            outHandles_.pop_back();
        }
    }

    auto squashed = std::make_unique<std::int32_t[]>(frames);

    for (std::size_t frame = 0; frame < frames; ++frame) {
        bool isFirst = true;
        for (auto& buffer : buffers) {
            if (isFirst) {
                isFirst = false;
                squashed[frame] = buffer[frame];
            } else {
                // mixing two channels (only for unsigned integers/floats):
                // Z = 2 * (A + B) - 2 * A * B / bit_width - bit_width
                std::uint64_t first = static_cast<std::int64_t>(squashed[frame]) - INT32_MIN;
                std::uint64_t second = static_cast<std::int64_t>(buffer[frame]) - INT32_MIN;
                squashed[frame] = std::clamp<std::uint64_t>(2 * (first + second) 
                    - std::max(first, second) / (UINT64_MAX / 2) * std::min(first, second)
                    - UINT64_MAX + INT32_MIN,
                INT32_MIN, INT32_MAX);
            }
        }

        if (buffers.empty()) {
            // put silence
            squashed[frame] = Silence;
        }
    }

    return squashed;
}