// STD
#include "pulse/channelmap.h"
#include "pulse/context.h"
#include "pulse/def.h"
#include "pulse/mainloop-api.h"
#include "pulse/mainloop.h"
#include "pulse/stream.h"
#include <cmath>
#include <memory>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <filesystem>

// laar
#include <src/ssd/macros.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

// pulse
#include <pulse/pulseaudio.h>

// AudioFile (WAV)
#include <AudioFile.h>

struct Data {
    AudioFile<std::int32_t> f;
    std::size_t wpos = 0;
    std::size_t avail = 0;
};

void killer(pa_context* c, void* userdata) {
    UNUSED(c);

    pa_mainloop_api* a = reinterpret_cast<pa_mainloop_api*>(userdata);
    std::cout << "drain complete, aborting mainloop";
    a->quit(a, 0);
}

void streamStateWatch(pa_stream* s, void* userdata) {
    pa_mainloop_api* a = reinterpret_cast<pa_mainloop_api*>(userdata);
    pa_context* c = pa_stream_get_context(s);

    switch(pa_stream_get_state(s)) {
        case PA_STREAM_CREATING:
            std::cout << "stream is connecting to server\n";
            return;
        case PA_STREAM_READY:
            std::cout << "stream is ready to read data\n";
            return;
        case PA_STREAM_TERMINATED:
            std::cout << "stream closing, draining context\n";
            pa_context_drain(c, killer, a);
        default:
            return;
    }
}

void contextStateWatch(pa_context* c, void* userdata) {
    pa_mainloop_api* a = reinterpret_cast<pa_mainloop_api*>(userdata);

    switch(pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
            std::cout << "context is connecting to server\n";
            return;
        case PA_CONTEXT_READY:
            std::cout << "context is ready to read data\n";
            return;
        case PA_CONTEXT_FAILED:
            a->quit(a, 1);
            return;
        default:
            return;
    }
}

void write(pa_stream* s, unsigned long bytes, void* userdata) {
    Data* data = reinterpret_cast<Data*>(userdata);
    void* buf;
    std::size_t n = bytes;
    pa_stream_begin_write(s, &buf, &n);

    if (data->avail == 0) {
        // first write
        data->avail = data->f.getNumSamplesPerChannel();
    }

    if (data->avail == data->wpos) {
        // buffer exhausted
        pa_stream_disconnect(s);
        return;
    }

    auto end = std::min(data->avail, data->wpos + n);
    auto samples = std::make_unique<std::int16_t[]>(end - data->wpos);
    for (std::size_t sample = data->wpos; sample < end; ++sample) {
        samples[sample] = data->f.samples[0][sample];
    }

    std::memcpy(buf, samples.get(), n);
    pa_stream_write(s, samples.get(), n, nullptr, 0, PA_SEEK_RELATIVE);
}

int main() {
    // if (!pcm_log::configureLogging(&std::cerr).ok()) {
    //     return 1;
    // }

    std::cout << "example demonstrates the usage of Pulseaudio async API";

    pa_mainloop* m = pa_mainloop_new();
    pa_mainloop_api* a = pa_mainloop_get_api(m);

    pa_context* c = pa_context_new(a, "laar-example");
    pa_context_ref(c);
    pa_context_connect(c, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
    pa_context_set_state_callback(c, contextStateWatch, a);

    std::string filename;
    std::cout << "Enter file: "; std::cin >> filename;

    if (!std::filesystem::exists(filename)) {
        std::cout << "File: " << filename << " does not exist. aborting. \n";
        return 1;
    }

    std::cout << "Loading file. summary: \n";
    AudioFile<std::int32_t> file (filename);
    file.printSummary();

    Data data;
    data.f = std::move(file);

    auto spec = std::make_unique<pa_sample_spec>();
    spec->rate = 44100;
    spec->channels = 1;
    spec->format = PA_SAMPLE_S16LE;

    auto attr = std::make_unique<pa_buffer_attr>();
    attr->prebuf = spec->rate;

    pa_channel_map* map = new pa_channel_map;
    pa_channel_map_init_auto(map, 1, PA_CHANNEL_MAP_ALSA);

    pa_stream* s = pa_stream_new(c, "laar-stream", spec.get(), map);
    pa_stream_ref(s);
    pa_stream_connect_playback(s, nullptr, attr.get(), PA_STREAM_NOFLAGS, nullptr, nullptr);
    pa_stream_set_state_callback(s, streamStateWatch, a);
    pa_stream_set_write_callback(s, write, &data);

    int retval = 0;
    pa_mainloop_run(m, &retval);

    delete map;

    pa_context_unref(c);
    pa_stream_unref(s);
    pa_mainloop_free(m);

    return retval;
}