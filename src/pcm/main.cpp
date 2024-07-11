#include <chrono>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>

// protos
#include <protos/client/simple/simple.pb.h>
#include <protos/client/client-message.pb.h>

#include <pulse/def.h>
#include <pulse/sample.h>
#include <pulse/simple.h>

#include <AudioFile.h>

// local
#include <pcm/sync-protocol/sync-protocol.hpp>

// sockpp
#include <sockpp/tcp_connector.h>
#include <thread>

enum EModes {
    MODE_EXPONENT = 0,
    MODE_WAV_FILE = 1,
    MODE_SINE = 2,
};

int playExponent() {

    std::cout << "This is demo for playback on laar SSD, it plays exponent-like sound \n"
              << "samples on playback, with 44100 sample rate using unsigned 32 bite wide sample type. \n";

    int duration = 0;
    std::cout << "enter duration (between 1 and 120, in seconds): "; std::cin >> duration;
    duration = std::clamp(duration, 1, 120);
    int period = 0;
    std::cout << "period specifies how intensive sound is and how many samples \n"
              << "will be transferred at once. \n";
    std::cout << "enter period (between 100 and 2000, in samples): "; std::cin >> period; 
    period = std::clamp(period, 100, 2000);

    auto spec = std::make_unique<pa_sample_spec>();
    spec->rate = 44100;
    spec->channels = 1;
    spec->format = PA_SAMPLE_S32LE;
    std::cout << "Sound spec: 44100 sample rate, PA_SIMPLE_S32LE sample type, Mono. \n";

    auto attr = std::make_unique<pa_buffer_attr>();
    attr->prebuf = spec->rate;
    std::cout << "Program also asks to halt playback until at least %rate% (set to 44100) samples \n"
              << "will be transferred. This is done to ensure the will be no underruns on server. \n";

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
    std::cout << "Programs establishes connection, passing the arguments we declared earlier and \n"
              << "and some other purely decorative settings (at least in basic configuration). \n";

    auto data = std::make_unique<std::int32_t[]>(spec->rate * duration);
    std::cout << "Assembling buffer to store one period at a time. \n";
    std::size_t samples = 0, periodsTotal = spec->rate * duration / period;
    for (std::size_t i = 0; i < periodsTotal; ++i) {
        for (std::size_t j = 0; j < period; ++j) {
            auto val = std::clamp<int>(std::sin((double) j / period * std::numbers::pi * 2) * INT32_MAX / 4 - INT32_MIN, INT32_MIN, INT32_MAX);
            data[i * period + j] = val;
        }
        pa_simple_write(connection, data.get() + i * period, period, nullptr);
    }

    std::cout << "Transfer complete! Program sent periods " << periodsTotal << " times! \n";
    std::cout << "Total samples sent: " << samples << "\n";

    pa_simple_free(connection);
    std::cout << "Connection was closed. Server will finish on this buffer, and then close it. \n";

    return 0;
}

int playWav() {

    std::cout << "This is demo for playback on laar SSD, it plays .wav file. \n";

    std::string filename;
    std::cout << "Enter file: "; std::cin >> filename;

    if (!std::filesystem::exists(filename)) {
        std::cout << "File: " << filename << " does not exist. aborting. \n";
        return 1;
    }

    std::cout << "Loading file. summary: \n";
    AudioFile<std::int32_t> file (filename);
    file.printSummary();

    int period = 0;
    std::cout << "period specifies how intensive sound is and how many samples \n"
              << "will be transferred at once. \n";
    std::cout << "enter period (between 100 and 2000, in samples): "; std::cin >> period; 
    period = std::clamp(period, 100, 2000);

    auto spec = std::make_unique<pa_sample_spec>();
    spec->rate = 44100;
    spec->channels = 1;
    spec->format = PA_SAMPLE_S16LE;

    auto attr = std::make_unique<pa_buffer_attr>();
    attr->prebuf = spec->rate;

    auto connection = pa_simple_new(
        "localhost", 
        "example-client", 
        PA_STREAM_PLAYBACK, 
        nullptr, 
        "song-stream", 
        spec.get(), 
        nullptr, 
        attr.get(), 
        nullptr
    );

    auto data = std::make_unique<std::int16_t[]>(period);
    auto samples = file.getNumSamplesPerChannel();
    for (int sample = 0; sample < samples; sample += period) {
        std::memset(data.get(), 0, period);
        for (int j = sample; j < std::min(sample + period, samples); ++j) {
            data[j % period] = file.samples[0][j];
        }
        pa_simple_write(connection, data.get(), period, nullptr);
    }

    pa_simple_free(connection);

    return 0;
}


int main() {
    
    std::cout << "What mode you want to run? \n";
    std::cout << " - sample exponent-like simple sound test (0) \n";
    std::cout << " - play sound file on server (1) \n";

    int option; std::cin >> option;

    switch (option) {
        case MODE_EXPONENT:
            return playExponent();
        case MODE_WAV_FILE:
            return playWav();
        default:
            std::cout << "Option: " << option << " is unknown. aborting. \n";
    }
    return 1;
}