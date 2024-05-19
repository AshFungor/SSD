#include "pulse/def.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>

// protos
#include <numbers>
#include <protos/client/simple/simple.pb.h>
#include <protos/client/client-message.pb.h>

#include <pulse/sample.h>
#include <pulse/simple.h>
#include <pcm/sync-protocol/sync-protocol.hpp>

// sockpp
#include <sockpp/tcp_connector.h>

int main() {

    const int duration = 5; // in seconds
    const int period = 1200;

    std::cout << "Demo for playback on SSD";

    auto spec = std::make_unique<pa_sample_spec>();
    spec->rate = 44100;
    spec->channels = 1;
    spec->format = PA_SAMPLE_S32LE;

    auto attr = std::make_unique<pa_buffer_attr>();
    attr->prebuf = spec->rate;

    auto connection = pa_simple_new(
        "localhost", 
        "example-client", 
        PA_STREAM_PLAYBACK, 
        nullptr, 
        "example-stream", 
        spec.get(), 
        nullptr, 
        attr.get(), 
        nullptr
    );

    auto data = std::make_unique<std::int32_t[]>(spec->rate * duration);
    std::size_t samples = 0;
    for (std::size_t i = 0; i < spec->rate * duration / period; ++i) {
        for (std::size_t j = 0; j < period; ++j) {
            auto val = INT32_MAX / (j + 1);
            data[i * period + j] = val;
        }
        pa_simple_write(connection, data.get() + i * period, period, nullptr);
        samples += period;
    }
    std::cout << "samples sent: " << samples;
   
    pa_simple_free(connection);

    return 0;
}