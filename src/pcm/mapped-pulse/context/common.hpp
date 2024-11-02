#pragma once

// pulse
#include "protos/server-message.pb.h"
#include "pulse/def.h"
#include "pulse/mainloop-api.h"
#include "pulse/operation.h"
#include "pulse/xmalloc.h"
#include "src/ssd/core/message.hpp"
#include <memory>
#include <pulse/context.h>

// abseil
#include <absl/strings/str_format.h>

// laar
#include <queue>
#include <src/ssd/macros.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

namespace laar {

    struct CallbackWrapper {
        pa_context_notify_cb_t cb;
        void* userdata;
    };

}

struct pa_operation {
    pa_operation_state_t state;
    pa_operation_notify_cb_t cbNotify;
    void* userdata;
};

struct pa_context {

    struct QueuedMessage {
        NSound::TServiceMessage message;
        pa_operation* op;
    };

    std::deque<QueuedMessage> outQueue;

    struct Callbacks {
        laar::CallbackWrapper notify;
    } callbacks;

    struct NetworkState {
        // message handling
        std::shared_ptr<laar::MessageFactory> factory;

        // low-level IO
        int fd;
        std::unique_ptr<std::uint8_t[]> buffer;
        std::size_t size; 
    } network;

    struct State {
        pa_mainloop_api* api;
        pa_context_state_t state;
        int refs;
        int error;
    } state;
};

namespace laar {

    inline void updateOp(pa_operation* op, pa_operation_state_t state) {
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(op));

        op->state = state;
        if (op->cbNotify) {
            op->cbNotify(op, op->userdata);
        }
    }

    inline NSound::TServiceMessageStream parseQueue(pa_context* context) {
        NSound::TServiceMessageStream stream;
        for (std::deque<pa_context::QueuedMessage>::iterator iter = context->outQueue.begin(); iter != context->outQueue.end(); ++iter) {
            stream.mutable_stream()->Add(std::move(iter->message));
        }
        return stream;
    }

}