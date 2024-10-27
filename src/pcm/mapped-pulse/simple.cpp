// Abseil
#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_format.h>

// boost
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/system_error.hpp>

// pulse
#include <pulse/def.h>
#include <pulse/sample.h>
#include <pulse/simple.h>
#include <pulse/xmalloc.h>

// STD
#include <thread>
#include <memory>
#include <cstdint>
#include <cstring>
#include <sstream>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/header.hpp>
#include <src/ssd/sound/converter.hpp>
#include <src/pcm/mapped-pulse/simple.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

// protos
#include <protos/client/base.pb.h>
#include <protos/service/base.pb.h>
#include <protos/client-message.pb.h>
#include <protos/server-message.pb.h>

namespace {

    boost::asio::const_buffer makeRequest(NSound::TClientMessage message, std::unique_ptr<std::uint8_t[]>& buffer) {
        auto header = laar::Message::make(message.ByteSizeLong());
        header.writeToArray(buffer.get());
        message.SerializeToArray(buffer.get() + header.getHeaderSize(), header.getPayloadSize());
        return boost::asio::const_buffer(buffer.get(), header.getHeaderSize() + header.getPayloadSize());
    }

    absl::Status wrapServerCall(pa_simple* connection, NSound::TClientMessage message) {
        try {
            connection->socket->write_some(makeRequest(std::move(message), connection->buffer));
            return absl::OkStatus();
        } catch (const boost::system::system_error& error) {
            return absl::InternalError(error.what());
        } catch (...) {
            return absl::InternalError("unknown error");
        }
    }

    absl::StatusOr<NSound::TServiceMessage> wrapServerResponse(pa_simple* connection) {
        try {
            std::stringstream iss {reinterpret_cast<char*>(connection->buffer.get()), std::ios::in | std::ios::binary};
            connection->socket->read_some(boost::asio::mutable_buffer(connection->buffer.get(), laar::Message::getHeaderSize()));
            auto header = laar::Message::readFromArray(iss);
            connection->socket->read_some(boost::asio::mutable_buffer(connection->buffer.get(), header.getPayloadSize()));
            NSound::TServiceMessage response;
            if (bool parseStatus = response.ParseFromArray(connection->buffer.get(), header.getPayloadSize()); !parseStatus) {
                return absl::InternalError("failed tp parse message from stream");
            }
            return response;
        } catch (const boost::system::system_error& error) {
            return absl::InternalError(error.what());
        } catch (...) {
            return absl::InternalError("unknown error");
        }
    }

}

pa_simple* __internal_pcm::simple(
    const char* /* server */,
    const char* name,
    pa_stream_direction_t dir,
    const char* /* dev */,
    const char* stream_name,
    const pa_sample_spec* ss,
    const pa_channel_map* /* map */,
    const pa_buffer_attr* attr,
    int* /* error */) 
{
    NSound::NCommon::TStreamConfiguration config;

    config.set_clientname(name);
    config.set_streamname(stream_name);

    if (dir == PA_STREAM_PLAYBACK) {
        config.set_direction(NSound::NCommon::TStreamConfiguration::PLAYBACK);
    } else if (dir == PA_STREAM_RECORD) {
        config.set_direction(NSound::NCommon::TStreamConfiguration::RECORD);
    } else {
        pcm_log::log(absl::StrFormat("unsupported stream direction: %d", dir), pcm_log::ELogVerbosity::ERROR);
    }

    if (ss) {

        if (ss->rate == laar::BaseSampleRate) {
            config.mutable_samplespec()->set_samplerate(laar::BaseSampleRate);
        } else {
            pcm_log::log(absl::StrFormat("unsupported rate: %d", ss->rate), pcm_log::ELogVerbosity::ERROR);
        }

        if (ss->format == PA_SAMPLE_U8) {
            config.mutable_samplespec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::UNSIGNED_8);
        } else if (ss->format == PA_SAMPLE_S16LE) {
            config.mutable_samplespec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_16_LITTLE_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S16BE) {
            config.mutable_samplespec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_16_BIG_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S24LE) {
            config.mutable_samplespec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_24_LITTLE_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S24BE) {
            config.mutable_samplespec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_24_BIG_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S32LE) {
            config.mutable_samplespec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_32_LITTLE_ENDIAN);
        } else if (ss->format == PA_SAMPLE_S32BE) {
            config.mutable_samplespec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::SIGNED_32_BIG_ENDIAN);
        } else if (ss->format == PA_SAMPLE_FLOAT32LE) {
            config.mutable_samplespec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::FLOAT_32_LITTLE_ENDIAN);
        } else if (ss->format == PA_SAMPLE_FLOAT32BE) {
            config.mutable_samplespec()->set_format(NSound::NCommon::TStreamConfiguration::TSampleSpecification::FLOAT_32_BIG_ENDIAN);
        } else {
            pcm_log::log(absl::StrFormat("unsupported format: %d", ss->format), pcm_log::ELogVerbosity::ERROR);
        }

        if (ss->channels == 1) {
            config.mutable_samplespec()->set_channels(ss->channels);
        } else {
            pcm_log::log(absl::StrFormat("unsupported channel number: %d", ss->channels), pcm_log::ELogVerbosity::ERROR);
        }
    }

    if (attr) {
        config.mutable_bufferconfig()->set_prebuffingsize(attr->prebuf);
    }

    pa_simple* connection = pa_xnew(pa_simple, 1);
    std::construct_at(connection);

    connection->context = std::make_unique<boost::asio::io_context>();
    connection->config = std::move(config);

    boost::asio::ip::tcp::resolver resolver(*connection->context);
    boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), "127.0.0.1", std::to_string(laar::Port));
    boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

    connection->socket = std::make_unique<boost::asio::ip::tcp::socket>(*connection->context);
    connection->socket->connect(*iterator);

    NSound::TClientMessage message;
    message.mutable_basemessage()->mutable_streamconfiguration()->CopyFrom(connection->config);
    if (absl::Status status = wrapServerCall(connection, message); !status.ok()) {
        pcm_log::log(status.ToString(), pcm_log::ELogVerbosity::ERROR);
        return nullptr;
    }

    if (absl::StatusOr<NSound::TServiceMessage> response = wrapServerResponse(connection); !response.ok()) {
        pcm_log::log(response.status().ToString(), pcm_log::ELogVerbosity::ERROR);
        return nullptr;
    } else {
        if (!response->mutable_basemessage()->mutable_streamalteredconfiguration()->opened()) {
            pcm_log::log("server failed to open stream", pcm_log::ELogVerbosity::ERROR);
            return nullptr;
        }
        pcm_log::log("stream opened!", pcm_log::ELogVerbosity::INFO);
    }

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
    return __internal_pcm::simple(server, name, dir, dev, stream_name, ss, map, attr, error);
}

int __internal_pcm::close(pa_simple* connection) {
    NSound::TClientMessage message;
    message.mutable_basemessage()->mutable_directive()->set_type(NSound::NCommon::TStreamDirective::CLOSE);
    if (absl::Status status = wrapServerCall(connection, message); !status.ok()) {
        pcm_log::log(status.ToString(), pcm_log::ELogVerbosity::ERROR);
        return -1;
    }

    return 0;
}

void pa_simple_free(pa_simple *s) {
    __internal_pcm::close(s);
    pa_xfree(s);
}

int __internal_pcm::write(pa_simple* connection, const void* bytes, std::size_t size) {
    const auto& singleFrameBytes = __internal_pcm::bytesPerTimeFrame_;
    std::size_t byteSize = size * laar::getSampleSize(connection->config.samplespec().format());
    std::size_t bytesTransferred = 0;

    for (std::size_t frame = 0; frame < byteSize; frame += singleFrameBytes) {
        std::size_t current = (byteSize - frame < singleFrameBytes) ? byteSize - frame : singleFrameBytes;
        
        bytesTransferred += current;
        if (bytesTransferred >= singleFrameBytes) {
            std::this_thread::sleep_for(timeFrame_);
            bytesTransferred = 0;
        }

        auto buffer = std::make_unique<char[]>(singleFrameBytes);
        std::memcpy(buffer.get(), bytes, singleFrameBytes);
        NSound::TClientMessage message;
        
        message.mutable_basemessage()->mutable_push()->set_data(buffer.get(), singleFrameBytes);
        message.mutable_basemessage()->mutable_push()->set_datasamplesize(size);
        if (absl::Status status = wrapServerCall(connection, std::move(message)); !status.ok()) {
            pcm_log::log(status.ToString(), pcm_log::ELogVerbosity::ERROR);
            return -1;
        }
    }

    return 0;
}

int pa_simple_write(pa_simple *s, const void *data, size_t bytes, int *error) {
    int err = __internal_pcm::write(s, data, bytes);
    if (error) {
        *error = err;
    }
    return err;
}

int __internal_pcm::read(pa_simple* /* connection */, void* /* bytes */, std::size_t /* size */) {
    return -1;
}

int pa_simple_read(pa_simple *s, void *data, size_t bytes, int* error) {
    int err = __internal_pcm::read(s, data, bytes);
    if (error) {
        *error = err;
    }
    return err;
}

int __internal_pcm::drain(pa_simple* connection) {
    NSound::TClientMessage message;
    message.mutable_basemessage()->mutable_directive()->set_type(NSound::NCommon::TStreamDirective::DRAIN);
    if (absl::Status status = wrapServerCall(connection, message); !status.ok()) {
        pcm_log::log(status.ToString(), pcm_log::ELogVerbosity::ERROR);
        return -1;
    }

    return -1;
}

int pa_simple_drain(pa_simple* s, int* error) {
    int err = __internal_pcm::drain(s);
    if (error) {
        *error = err;
    }
    return err;
}

int __internal_pcm::flush(pa_simple* connection) {
    NSound::TClientMessage message;
    message.mutable_basemessage()->mutable_directive()->set_type(NSound::NCommon::TStreamDirective::FLUSH);
    if (absl::Status status = wrapServerCall(connection, message); !status.ok()) {
        pcm_log::log(status.ToString(), pcm_log::ELogVerbosity::ERROR);
        return -1;
    }

    return -1;
}

int pa_simple_flush(pa_simple* s, int* error) {
    int err = __internal_pcm::flush(s);
    if (error) {
        *error = err;
    }
    return err;
}

pa_usec_t pa_simple_get_latency(pa_simple* /* s */, int* /* error */) {
    return 0;
}