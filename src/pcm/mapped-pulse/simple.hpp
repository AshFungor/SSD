// #pragma once

// // pulse
// #include <chrono>
// #include <cstddef>
// #include <pulse/simple.h>

// // STD
// #include <memory>

// // laar
// #include <network/interfaces/i-message.hpp>

// struct pa_simple {
//     std::unique_ptr<sockpp::tcp_connector> connection;
//     float scale;
// };

// namespace __internal_pcm {

//     using hs_clock = std::chrono::high_resolution_clock;

//     inline constexpr int port = 5050;
//     inline constexpr std::size_t headerSize = 6;

//     // robust - 2 secs of playback, so
//     // 44000 * 2 / 1000 = 88 calls
//     // 1 / 88 => every 10 ms 1000 samples
//     // in reality this should be managed by server 
//     // via two-sided protocol, but this one works fine for now
//     inline constexpr std::size_t bytesPerTimeFrame_ = 1000;
//     inline constexpr std::chrono::milliseconds timeFrame_ (10);

//     class LoadBalancer {
//     public:

//         LoadBalancer(
//             std::size_t bytes,
//             std::chrono::milliseconds delta
//         );
//         // checks time frame and blocks
//         // if this message exceeds limit
//         void balance(std::size_t size);

//     private:
//         void reset(hs_clock::time_point check);
//         void hold(hs_clock::time_point goesOff);

//     private:
//         std::size_t bytes_;
//         std::chrono::milliseconds delta_;
//         // state
//         std::size_t current_;
//         hs_clock::time_point previous_;
//     };

//     std::pair<std::unique_ptr<char[]>, std::size_t> assembleStructuredMessage(
//         laar::IResult::EVersion version,
//         laar::IResult::EPayloadType payloadType,
//         laar::IResult::EType type,
//         google::protobuf::Message& message
//     );

//     inline static LoadBalancer balancer (bytesPerTimeFrame_, timeFrame_);

//     pa_simple* makeConnection(
//         const char *server,                 /**< Server name, or NULL for default */
//         const char *name,                   /**< A descriptive name for this client (application name, ...) */
//         pa_stream_direction_t dir,          /**< Open this stream for recording or playback? */
//         const char *dev,                    /**< Sink (resp. source) name, or NULL for default */
//         const char *stream_name,            /**< A descriptive name for this stream (application name, song title, ...) */
//         const pa_sample_spec *ss,           /**< The sample type to use */
//         const pa_channel_map *map,          /**< The channel map to use, or NULL for default */
//         const pa_buffer_attr *attr,         /**< Buffering attributes, or NULL for default */
//         int *error                          /**< A pointer where the error code is stored when the routine returns NULL. It is OK to pass NULL here. */
//     );

//     int syncRead(pa_simple* connection, void* bytes, std::size_t size);
//     int syncWrite(pa_simple* connection, const void* bytes, std::size_t size);
//     int SyncDrain(pa_simple* connection);
//     int SyncFlush(pa_simple* connection);
//     int SyncClose(pa_simple* connection);

//     int assembleAndWrite(pa_simple* connection, const void* bytes, std::size_t size);

// }