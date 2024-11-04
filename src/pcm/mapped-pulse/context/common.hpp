#pragma once

// STD
#include "pulse/stream.h"
#include <cstdint>
#include <memory>

// pulse
#include <pulse/def.h>
#include <pulse/sample.h>
#include <pulse/xmalloc.h>
#include <pulse/context.h>
#include <pulse/operation.h>
#include <pulse/channelmap.h>
#include <pulse/mainloop-api.h>

// abseil
#include <absl/strings/str_format.h>

// proto
#include <protos/server-message.pb.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/message.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

// protos
#include <protos/common/stream-configuration.pb.h>

namespace laar {

    // robust - 2 secs of playback, so
    // 44000 * 2 / 1000 = 88 calls
    // 1 / 88 => every 10 ms 1000 samples
    // in reality this should be managed by server 
    // via two-sided protocol, but this one works fine for now
    inline constexpr std::size_t SamplesPerTimeFrame = 1000;
    inline constexpr std::chrono::milliseconds TimeFrame (10);

    template<typename T>
    struct CallbackWrapper {
        T cb;
        void* userdata;
    };

}

struct pa_operation {
    void (*cbSuccess)(laar::Message message, void* owner);
    void* owner;

    int refs;
    pa_operation_state_t state;
    pa_operation_notify_cb_t cbNotify;
    void* userdata;
};

struct pa_stream {

    struct Callbacks {
        laar::CallbackWrapper<pa_stream_request_cb_t> write;
        laar::CallbackWrapper<pa_stream_request_cb_t> read;
        laar::CallbackWrapper<pa_stream_notify_cb_t> state;
        laar::CallbackWrapper<pa_stream_notify_cb_t> start;
        laar::CallbackWrapper<pa_stream_success_cb_t> drain;
    } callbacks;

    struct PulseAttributes {
        std::string name;
        pa_stream_direction dir;
        pa_channel_map map;
        pa_sample_spec spec;
        pa_buffer_attr buffer;
    } pulseAttributes;

    struct Buffer {
        std::unique_ptr<std::uint8_t[]> buffer;
        std::size_t size;
        std::size_t rPos;
        std::size_t wPos;
        std::size_t avail;
        bool directWrite;
    } buffer;

    struct Network {
        NSound::NCommon::TStreamConfiguration config;
        std::uint32_t id;
    } network;

    struct State {
        bool cork;
        pa_stream_state_t state;
        pa_context* context;
        int refs;

        pa_operation* drain;
        int ops;
    } state;

};

struct pa_context {

    struct QueuedMessage {
        laar::Message message;
        pa_operation* op;
    };

    std::list<QueuedMessage> out;

    struct Callbacks {
        laar::CallbackWrapper<pa_context_notify_cb_t> notify;
        laar::CallbackWrapper<pa_context_notify_cb_t> drained;
    } callbacks;

    struct NetworkState {
        // message handling
        std::shared_ptr<laar::MessageFactory> factory;
        
        int mode;
        std::size_t total;
        std::size_t expected;
        std::size_t current;

        // low-level IO
        int fd;
        std::unique_ptr<std::uint8_t[]> buffer;
        std::size_t size; 
    } network;

    struct State {
        pa_operation* drain;

        std::string name;
        pa_mainloop_api* api;
        pa_context_state_t state;
        int refs;
        int error;
    } state;

    struct Events {
        pa_io_event* iter;
    } events;
};

namespace laar {

    inline void updateOp(pa_operation* op, pa_operation_state_t state) {
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(op));

        if (state != PA_OPERATION_CANCELLED) {
            op->state = state;
            if (op->cbNotify) {
                op->cbNotify(op, op->userdata);
            }
        }
    }

}