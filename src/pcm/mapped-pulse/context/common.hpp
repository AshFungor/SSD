#pragma once

// STD
#include <memory>

// pulse
#include <pulse/def.h>
#include <pulse/xmalloc.h>
#include <pulse/context.h>
#include <pulse/operation.h>
#include <pulse/mainloop-api.h>

// abseil
#include <absl/strings/str_format.h>

// proto
#include <protos/server-message.pb.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/message.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

namespace laar {

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