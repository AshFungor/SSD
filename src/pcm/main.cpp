#include "pulse/def.h"
#include <cmath>
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

    std::cout << "Demo for playback on SSD";

    auto spec = std::make_unique<pa_sample_spec>();
    spec->rate = 44100;
    spec->channels = 1;
    spec->format = PA_SAMPLE_FLOAT32LE;
    auto connection = pa_simple_new("", "", PA_STREAM_PLAYBACK, nullptr, "", spec.get(), nullptr, nullptr, nullptr);

    auto data = std::make_unique<float[]>(spec->rate * duration);
    for (std::size_t i = 0; i <= spec->rate * duration; ++i) {
        if (i % 400 == 0 && i > 0) {
            pa_simple_write(connection, data.get() + i - 400, 400, nullptr);
        }
        data[i] = std::sin(2 * std::numbers::pi * i * spec->rate / 1200);
    }

   

    pa_simple_free(connection);

    return 0;
}