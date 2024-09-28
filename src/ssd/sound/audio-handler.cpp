// boost
#include <boost/asio/dispatch.hpp>

// laar
#include <src/ssd/sound/read-handle.hpp>
#include <src/ssd/sound/write-handle.hpp>
#include <src/ssd/util/config-loader.hpp>
#include <src/ssd/sound/audio-handler.hpp>
#include <src/ssd/sound/interfaces/i-audio-handler.hpp>
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
#include <mutex>
#include <cctype>
#include <memory>
#include <future>
#include <vector>
#include <cstdint>
#include <exception>
#include <algorithm>
#include <functional>

// plog
#include <plog/Log.h>
#include <plog/Severity.h>

// proto
#include <protos/client/base.pb.h>

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
    std::shared_ptr<boost::asio::io_context> context
) {
    return std::make_shared<SoundHandler>(std::move(configHandler), std::move(context), Private());    
}

SoundHandler::SoundHandler(
    std::shared_ptr<laar::ConfigHandler> configHandler,
   std::shared_ptr<boost::asio::io_context> context,
    Private access
)
    : context_(std::move(context))
    , configHandler_(std::move(configHandler))
    , bassDispatcher_(laar::BassRouterDispatcher::create(
        ESamplesOrder::NONINTERLEAVED, 
        ESamples::SIGNED_32_LITTLE_ENDIAN,
        BaseSampleRate,
        BassRouterDispatcher::BassRange(20, 250),
        BassRouterDispatcher::ChannelInfo(0, 1))
    )
    , local_(nullptr)
    , audio_(RtAudio::Api::LINUX_ALSA)
{}

void SoundHandler::init() {
    std::call_once(init_, [this](){
            local_ = makeLocalData();

            configHandler_->subscribeOnDefaultConfig(
                SOUND_SECTION, 
                [&](const auto& config) {
                    parseDefaultConfig(config);
                }, 
                weak_from_this()
            );

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
    );
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
        for (std::size_t i = 0; i < info.sampleRates.size(); ++i) {
            if (i == info.sampleRates.size() - 1) {
                absl::StrAppend(&available, absl::StrFormat("%d; ", info.sampleRates[i]));
            } else {
                absl::StrAppend(&available, absl::StrFormat("%d, ", info.sampleRates[i]));
            }
        }

        absl::StrAppend(
            &verdict, 
            absl::StrFormat(
                "device does not have %d sample rate available, only the following are supported: %s",
                laar::BaseSampleRate,
                available
            )
        );

        return false;
    }

    absl::StrAppend(&verdict, absl::StrFormat("device supports requested sample rate: %d; ", laar::BaseSampleRate));
    return true;
}

bool SoundHandler::checkSampleFormat(const RtAudio::DeviceInfo& info, std::string& verdict) noexcept {
    absl::StrAppend(&verdict, "native sample rates: ");
    for (const RtAudioFormat& format : {
        RTAUDIO_SINT8, 
        RTAUDIO_SINT16,
        RTAUDIO_SINT32,
        RTAUDIO_SINT24,
        RTAUDIO_FLOAT32,
        RTAUDIO_FLOAT64
    }) {
        if (format & info.nativeFormats) {
            absl::StrAppend(&verdict, absl::StrFormat("%d, ", format));
        }
    }

    if (RTAUDIO_SINT32 & info.nativeFormats) {
        absl::StrAppend(&verdict, absl::StrFormat("required format is native %d; ", RTAUDIO_SINT32));
    } else {
        absl::StrAppend(&verdict, absl::StrFormat("required format is non-native %d; ", RTAUDIO_SINT32));
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

    return data;
}

void SoundHandler::onError(std::exception error) {
    PLOG(plog::error) << "error in sound module: " << error.what();
    throw error;
}

int laar::duplexCallback(
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

    if (!handler->future_.valid()) {
        // future is empty, task was not run yet
        buffer = handler->squash(frames);
    } else {
        buffer = handler->future_.get();
    }

    for (std::size_t channel = 0; channel < 2; ++channel) {
        for (std::size_t sample = 0; sample < frames; ++sample) {
            result[channel * frames + sample] = buffer[sample];
        }
    }

    // request more data to dispatch it in async manner
    buffer = handler->squash(frames);
    handler->future_ = boost::asio::post(*handler->context_, 
        std::packaged_task<std::unique_ptr<std::int32_t[]>(std::unique_ptr<std::int32_t[]>)>(
            std::bind(&SoundHandler::dispatchAsync, handler.get(), std::move(buffer))
        )
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
    
    if (absl::Status status = handler->unfetter(result, frames); !status.ok()) {
        PLOG(plog::error) << "Read Callback: failed to unfetter data, aborting";
        std::abort();
    }

    return rtcontrol::SUCCESS;
}

void SoundHandler::parseDefaultConfig(const nlohmann::json& config) {
    settings_.isCaptureEnabled = config.value<bool>("isCaptureEnabled", true);
    settings_.isPlaybackEnabled = config.value<bool>("isPlaybackEnabled", true);
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

std::unique_ptr<std::int32_t[]> SoundHandler::squash(std::size_t frames) {
    std::vector<std::unique_ptr<std::int32_t[]>> buffers;
    {
        std::unique_lock<std::mutex> locked(local_->handlerLock);

        for (std::size_t i = 0; i < outHandles_.size(); ++i) {
            if (auto handle = outHandles_[i].lock()) {
                if (!handle->isAlive()) {
                    std::swap(outHandles_[i], outHandles_.back());
                    outHandles_.pop_back();
                    continue;
                }

                buffers.emplace_back(std::make_unique<std::int32_t[]>(frames));
                if (absl::StatusOr<int> bytes = handle->read(buffers.back().get(), frames); !bytes.ok()) {
                    PLOG(plog::warning) 
                        << "Write Callback: failed to get " << frames 
                        << " bytes from handle: " << handle.get() 
                        << "; error: " << bytes.status().message();
                }
            }
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

absl::Status SoundHandler::unfetter(std::int32_t* source, std::size_t frames) {
    std::unique_lock<std::mutex> locked(local_->handlerLock);

    for (std::size_t i = 0; i < inHandles_.size(); ++i) {
        if (auto handle = inHandles_[i].lock()) {
            if (!handle->isAlive()) {
                std::swap(inHandles_[i], inHandles_.back());
                inHandles_.pop_back();
                continue;
            }

            if (absl::StatusOr<int> bytes = handle->write(source, frames); !bytes.ok() || static_cast<unsigned int>(bytes.value()) < frames) {
                if (!bytes.ok()) {
                    PLOG(plog::warning) 
                        << "unfetter: encountered error while writing bytes to handle: "
                        << handle.get() << ", message: " << bytes.status().message();
                } else {
                    PLOG(plog::warning)
                        << "unfetter: only " << bytes.value() << " bytes were written to handle; expected " << frames;
                }
                if (absl::Status status = handle->flush(); !status.ok()) {
                    PLOG(plog::error) << "unfetter: failed to recover handle, aborting";
                    return absl::InternalError("failed to recover handle, unfetter failed");
                }
            }
        }
    }

    return absl::OkStatus();
}