#pragma once

// Abseil
#include <absl/status/status.h>

// pulse
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdint>
#include <pulse/simple.h>

// STD
#include <chrono>
#include <memory>
#include <cstddef>

// boost
#include <boost/asio.hpp>

// laar
#include <src/ssd/macros.hpp>

// protos
#include <protos/client/base.pb.h>
#include <protos/service/base.pb.h>


struct pa_simple {
    NSound::NCommon::TStreamConfiguration config;
    // network state
    std::unique_ptr<std::uint8_t[]> buffer;
    std::unique_ptr<boost::asio::io_context> context;
    std::unique_ptr<boost::asio::ip::tcp::socket> socket;
};

namespace __internal_pcm {

    using TClientBaseMessage = NSound::NClient::NBase::TBaseMessage;
    using TServiceBaseMessage = NSound::NService::NBase::TBaseMessage;

    // robust - 2 secs of playback, so
    // 44000 * 2 / 1000 = 88 calls
    // 1 / 88 => every 10 ms 1000 samples
    // in reality this should be managed by server 
    // via two-sided protocol, but this one works fine for now
    inline constexpr std::size_t bytesPerTimeFrame_ = 1000;
    inline constexpr std::chrono::milliseconds timeFrame_ (10);

    pa_simple* simple(
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

    int read(pa_simple* connection, void* bytes, std::size_t size);
    int write(pa_simple* connection, const void* bytes, std::size_t size);
    int drain(pa_simple* connection);
    int flush(pa_simple* connection);
    int close(pa_simple* connection);

}