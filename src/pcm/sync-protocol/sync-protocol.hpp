#pragma once

// pulse
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

struct pa_simple {
    std::unique_ptr<sockpp::tcp_connector> connection;
    float scale;
};

namespace __internal_pcm {

    inline constexpr int port = 5050;
    inline constexpr std::size_t headerSize = 6;

    std::pair<std::unique_ptr<char[]>, std::size_t> assembleStructuredMessage(
        laar::IResult::EVersion version,
        laar::IResult::EPayloadType payloadType,
        laar::IResult::EType type,
        google::protobuf::Message& message
    );

    pa_simple* makeConnection(
        const char *server,                 /**< Server name, or NULL for default */
        const char *name,                   /**< A descriptive name for this client (application name, ...) */
        pa_stream_direction_t dir,          /**< Open this stream for recording or playback? */
        const char *dev,                    /**< Sink (resp. source) name, or NULL for default */
        const char *stream_name,            /**< A descriptive name for this stream (application name, song title, ...) */
        const pa_sample_spec *ss,           /**< The sample type to use */
        const pa_channel_map *map,          /**< The channel map to use, or NULL for default */
        const pa_buffer_attr *attr,         /**< Buffering attributes, or NULL for default */
        int *error                          /**< A pointer where the error code is stored when the routine returns NULL. It is OK to pass NULL here. */
    );

    int syncRead(pa_simple* connection, void* bytes, std::size_t size);
    int syncWrite(pa_simple* connection, const void* bytes, std::size_t size);
    int SyncDrain(pa_simple* connection);
    int SyncFlush(pa_simple* connection);
    int SyncClose(pa_simple* connection);

}