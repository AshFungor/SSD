// Abseil
#include <absl/status/status.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>

// pulse
#include <pulse/def.h>
#include <pulse/sample.h>
#include <pulse/simple.h>
#include <pulse/xmalloc.h>

// STD
#include <memory>
#include <format>
#include <thread>
#include <cstring>
#include <utility>

// Grpc
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/impl/call_op_set.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/sound/converter.hpp>
#include <src/pcm/mapped-pulse/simple.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

// protos
#include <protos/client/base.pb.h>
#include <protos/service/base.pb.h>
#include <protos/client-message.pb.h>
#include <protos/server-message.pb.h>
#include <protos/services/sound-router.grpc.pb.h>

pa_simple* __internal_pcm::makeSyncConnection(
    const char *server,
    const char *name,
    pa_stream_direction_t dir,
    const char *dev,
    const char *stream_name,
    const pa_sample_spec *ss,
    const pa_channel_map *map,
    const pa_buffer_attr *attr,
    int *error) 
{
    TClientBaseMessage::TStreamConfiguration config;

    config.set_clientname(name);
    config.set_streamname(stream_name);

    if (dir == PA_STREAM_PLAYBACK) {
        config.set_direction(TClientBaseMessage::TStreamConfiguration::PLAYBACK);
    } else if (dir == PA_STREAM_RECORD) {
        config.set_direction(TClientBaseMessage::TStreamConfiguration::RECORD);
    } else {
        int casted = dir;
        pcm_log::logErrorSilent("unsupported stream direction: {}", std::make_format_args(casted));
    }

    if (ss) {

        if (ss->rate == laar::BaseSampleRate) {
            config.mutable_samplespec()->set_samplerate(laar::BaseSampleRate);
        } else {
            pcm_log::logErrorSilent("unsupported rate: {}", std::make_format_args(ss->rate));
        }

        if (ss->format == PA_SAMPLE_U8) {
            config.mutable_samplespec()->set_format(TClientBaseMessage::TStreamConfiguration::TSampleSpecification::UNSIGNED_8);
        } else if (ss->format == PA_SAMPLE_S16LE) {
            config.mutable_samplespec()->set_format(TClientBaseMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_LITTLE_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S16BE) {
            config.mutable_samplespec()->set_format(TClientBaseMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_BIG_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S24LE) {
            config.mutable_samplespec()->set_format(TClientBaseMessage::TStreamConfiguration::TSampleSpecification::SIGNED_24_LITTLE_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S24BE) {
            config.mutable_samplespec()->set_format(TClientBaseMessage::TStreamConfiguration::TSampleSpecification::SIGNED_24_BIG_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S32LE) {
            config.mutable_samplespec()->set_format(TClientBaseMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S32BE) {
            config.mutable_samplespec()->set_format(TClientBaseMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_BIG_ENDIAN);
        } else if (ss->format == PA_SAMPLE_FLOAT32LE) {
            config.mutable_samplespec()->set_format(TClientBaseMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_LITTLE_ENDIAN);
        } else if (ss->format == PA_SAMPLE_FLOAT32BE) {
            config.mutable_samplespec()->set_format(TClientBaseMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_BIG_ENDIAN);
        } else {
            int format = ss->format;
            pcm_log::logErrorSilent("unsupported sample type: ", std::make_format_args(format));
        }

        if (ss->channels == 1) {
            config.mutable_samplespec()->set_channels(ss->channels);
        } else {
            pcm_log::logErrorSilent("unsupported channel number: {}", std::make_format_args(ss->channels));
        }
    }

    if (attr) {
        config.mutable_bufferconfig()->set_prebuffingsize(attr->prebuf);
    }

    pa_simple* connection = pa_xnew(pa_simple, 1);
    std::construct_at(connection);

    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(absl::StrFormat("localhost:%d", laar::port), grpc::InsecureChannelCredentials());
    auto context = std::make_unique<grpc::ClientContext>(); 
    connection->stub = NSound::NServices::SoundStreamRouter::NewStub(channel);
    connection->config = config;

    NSound::TClientMessage out;
    out.mutable_basemessage()->mutable_streamconfiguration()->CopyFrom(std::move(config));

    auto stream = connection->stub->RouteStream(context.get());
    stream->WriteLast(out, grpc::WriteOptions());
    
    NSound::TServiceMessage response;
    bool result = stream->Read(&response);
    if (result) {
        pcm_log::logErrorSilent("did not expect answer from server", std::make_format_args());
        return nullptr;
    }

    if (auto serverStatus = stream->Finish(); serverStatus.ok()) {
        return connection;
    } else {
        pcm_log::logErrorSilent(absl::StrCat("error on stream close: ", serverStatus.error_message()), std::make_format_args());
        return nullptr;
    }

    return nullptr;
}

pa_simple* pa_simple_new(
    const char *server,
    const char *name,
    pa_stream_direction_t dir,
    const char *dev,
    const char *stream_name,
    const pa_sample_spec *ss,
    const pa_channel_map *map,
    const pa_buffer_attr *attr,
    int *error) 
{
    return __internal_pcm::makeSyncConnection(server, name, dir, dev, stream_name, ss, map, attr, error);
}

int __internal_pcm::SyncClose(pa_simple* connection) {
    TClientBaseMessage::TStreamDirective directive;
    directive.set_type(TClientBaseMessage::TStreamDirective::CLOSE);
    return 0;
}

void pa_simple_free(pa_simple *s) {
    __internal_pcm::SyncClose(s);
    pa_xfree(s);
}

int __internal_pcm::syncWrite(pa_simple* connection, const void* bytes, std::size_t size) {
    const auto& singleFrameBytes = __internal_pcm::bytesPerTimeFrame_;
    std::size_t byteSize = size * laar::getSampleSize(connection->config.samplespec().format());

    NSound::TServiceMessage response;
    auto context = std::make_unique<grpc::ClientContext>();
    auto stream = connection->stub->RouteStream(context.get());
    for (std::size_t frame = 0; frame < byteSize; frame += singleFrameBytes) {
        std::size_t current = (byteSize - frame < singleFrameBytes) ? byteSize - frame : singleFrameBytes;
        
        connection->balancing.bytesTransferredOnFrame += current;
        if (connection->balancing.bytesTransferredOnFrame >= singleFrameBytes) {
            stream->WritesDone();
            if (auto status = stream->Finish(); !status.ok()) {
                const auto& error = status.error_message(); 
                pcm_log::logErrorSilent("error on grpc finish: {}", std::make_format_args(error));
                return 1;
            }
            std::this_thread::sleep_for(__internal_pcm::timeFrame_);
            context = std::make_unique<grpc::ClientContext>();
            stream = connection->stub->RouteStream(context.get());
            connection->balancing.bytesTransferredOnFrame = 0;
        }

        auto buffer = std::make_unique<char[]>(singleFrameBytes);
        std::memcpy(buffer.get(), bytes, singleFrameBytes);
        NSound::TClientMessage message;
        
        message.mutable_basemessage()->mutable_push()->set_data(buffer.get(), singleFrameBytes);
        message.mutable_basemessage()->mutable_push()->set_datasamplesize(size);
        stream->Write(std::move(message));
    }

    return 0;
}

int pa_simple_write(pa_simple *s, const void *data, size_t bytes, int *error) {
    return __internal_pcm::syncWrite(s, data, bytes);
}

int __internal_pcm::syncRead(pa_simple* connection, void* bytes, std::size_t size) {
    return -1;
}

int pa_simple_read(pa_simple *s, void *data, size_t bytes, int *error) {
    return __internal_pcm::syncRead(s, data, bytes);
}

int __internal_pcm::SyncDrain(pa_simple* connection) {
    return -1;
}

int pa_simple_drain(pa_simple *s, int *error) {
    return __internal_pcm::SyncDrain(s);
}

int __internal_pcm::SyncFlush(pa_simple* connection) {
    return -1;
}

int pa_simple_flush(pa_simple *s, int *error) {
    return __internal_pcm::SyncFlush(s);
}

pa_usec_t pa_simple_get_latency(pa_simple *s, int *error) {
    return 0;
}