// pulse
#include <cstdint>
#include <format>
#include <pulse/def.h>
#include <pulse/sample.h>
#include <pulse/simple.h>

// STD
#include <memory>

// sockpp
#include <sockpp/tcp_connector.h>
#include <sockpp/inet_address.h>
#include <sockpp/tcp_acceptor.h>
#include <sockpp/tcp_socket.h>

// laar
#include <network/interfaces/i-message.hpp>
#include <sounds/interfaces/i-audio-handler.hpp>
#include <utility>

// local
#include "sync-protocol.hpp"
#include <pcm/trace/trace.hpp>
#include "protos/client/client-message.pb.h"
#include "protos/client/simple/simple.pb.h"
#include "sounds/converter.hpp"

std::pair<std::unique_ptr<char[]>, std::size_t> __internal_pcm::assembleStructuredMessage(
    laar::IResult::EVersion version,
    laar::IResult::EPayloadType payloadType,
    laar::IResult::EType type,
    google::protobuf::Message& message)
{
    std::size_t len = message.ByteSizeLong();

    auto buffer = std::make_unique<char[]>(len);
    message.SerializeToArray(buffer.get(), len);

    auto out = std::make_unique<char[]>(headerSize + len);
    std::size_t section = 0;

    section = (std::uint32_t) version;
    section |= ((std::uint32_t) payloadType) << 4;
    section |= (((std::uint32_t) type)) << 8;

    section |= ((std::size_t) len) << 16;

    std::memcpy(out.get(), &section, headerSize);
    std::memcpy(out.get() + headerSize, buffer.get(), len);

    return std::make_pair(std::move(out), len + headerSize);
}

pa_simple* makeConnection(
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
    using namespace NSound::NSimple;
    
    TSimpleMessage::TStreamConfiguration config;

    config.set_client_name(name);
    config.set_stream_name(stream_name);

    if (dir == PA_STREAM_PLAYBACK) {
        config.set_direction(TSimpleMessage::TStreamConfiguration::PLAYBACK);
    } else if (dir == PA_STREAM_RECORD) {
        config.set_direction(TSimpleMessage::TStreamConfiguration::RECORD);
    } else {
        pcm_log::logError("unsupported stream direction", {});
    }

    float scale = 1;
    if (ss) {

        if (ss->rate == laar::BaseSampleRate) {
            config.mutable_sample_spec()->set_sample_rate(laar::BaseSampleRate);
        } else {
            pcm_log::logError("unsupported rate", {});
        }

        if (ss->format == PA_SAMPLE_U8) {
            config.mutable_sample_spec()->set_format(TSimpleMessage::TStreamConfiguration::TSampleSpecification::UNSIGNED_8);
            scale = 1;
        } else if (ss->format == PA_SAMPLE_S16LE) {
            config.mutable_sample_spec()->set_format(TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_LITTLE_ENDIAN);
            scale = sizeof (std::int16_t);
        } else if (ss->format == PA_SAMPLE_S16BE) {
            config.mutable_sample_spec()->set_format(TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_16_BIG_ENDIAN);
            scale = sizeof (std::int16_t);
        } else if (ss->format == PA_SAMPLE_S24LE) {
            config.mutable_sample_spec()->set_format(TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_24_LITTLE_ENDIAN);
            scale = 3;
        } else if (ss->format == PA_SAMPLE_S24BE) {
            config.mutable_sample_spec()->set_format(TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_24_BIG_ENDIAN);
            scale = 3;
        } else if (ss->format == PA_SAMPLE_S32LE) {
            config.mutable_sample_spec()->set_format(TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN);
            scale = sizeof (std::int32_t);
        } else if (ss->format == PA_SAMPLE_S32BE) {
            config.mutable_sample_spec()->set_format(TSimpleMessage::TStreamConfiguration::TSampleSpecification::SIGNED_32_BIG_ENDIAN);
            scale = sizeof (std::int32_t);
        } else if (ss->format == PA_SAMPLE_FLOAT32LE) {
            config.mutable_sample_spec()->set_format(TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_LITTLE_ENDIAN);
            scale = sizeof (float);
        } else if (ss->format == PA_SAMPLE_FLOAT32BE) {
            config.mutable_sample_spec()->set_format(TSimpleMessage::TStreamConfiguration::TSampleSpecification::FLOAT_32_BIG_ENDIAN);
            scale = sizeof (float);
        } else {
            pcm_log::logError("unsupported sample type", {});
        }

        if (ss->channels == 1) {
            config.mutable_sample_spec()->set_channels(ss->channels);
        } else {
            pcm_log::logError("unsupported channel number", {});
        }
    }

    if (attr) {
        config.mutable_buffer_config()->set_prebuffing_size(attr->prebuf);
    }

    pa_simple* connection = new pa_simple;
    connection->scale = scale;
    connection->connection = std::make_unique<sockpp::tcp_connector>();
    connection->connection->connect(sockpp::inet_address("localhost", __internal_pcm::port));

    NSound::TClientMessage out;
    out.mutable_simple_message()->mutable_stream_config()->CopyFrom(std::move(config));

    auto initMessage = __internal_pcm::assembleStructuredMessage(
        laar::IResult::EVersion::FIRST, 
        laar::IResult::EPayloadType::STRUCTURED, 
        laar::IResult::EType::OPEN_SIMPLE, 
        out
    );

    connection->connection->write_n(initMessage.first.get(), initMessage.second);

    return connection;
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
    return makeConnection(server, name, dir, dev, stream_name, ss, map, attr, error);
}

int __internal_pcm::SyncClose(pa_simple* connection) {
    NSound::NSimple::TSimpleMessage::TClose out;
    auto message = __internal_pcm::assembleStructuredMessage(
        laar::IResult::EVersion::FIRST, 
        laar::IResult::EPayloadType::STRUCTURED, 
        laar::IResult::EType::CLOSE_SIMPLE, 
        out
    );
    connection->connection->write_n(message.first.get(), message.second);
    return 0;
}

void pa_simple_free(pa_simple *s) {
    __internal_pcm::SyncClose(s);
    s->connection->close();
    free(s);
}

int __internal_pcm::syncWrite(pa_simple* connection, const void* bytes, std::size_t size) {
    NSound::NSimple::TSimpleMessage::TPush push;

    push.set_size(size);

    push.set_pushed((char*) bytes, (int) size * connection->scale);

    NSound::TClientMessage out;
    out.mutable_simple_message()->mutable_push()->CopyFrom(std::move(push));

    std::cout << "size: " << out.ByteSizeLong() << "\n";

    auto message = __internal_pcm::assembleStructuredMessage(
        laar::IResult::EVersion::FIRST, 
        laar::IResult::EPayloadType::STRUCTURED, 
        laar::IResult::EType::PUSH_SIMPLE, 
        out
    );

    connection->connection->write_n(message.first.get(), message.second);
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