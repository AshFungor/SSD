// pulse
#include "protos/server-message.pb.h"
#include "pulse/def.h"
#include "pulse/mainloop-api.h"
#include "pulse/xmalloc.h"
#include "src/ssd/core/message.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <pulse/context.h>
#include <src/pcm/mapped-pulse/context/common.hpp>

// abseil
#include <absl/strings/str_format.h>

// laar
#include <queue>
#include <src/ssd/macros.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>
#include <sys/socket.h>

namespace {

    void iterate(pa_mainloop_api* a, pa_defer_event* e, void* userdata) {
        pa_context* c = reinterpret_cast<pa_context*>(userdata);

        
    }

    void changeContextState(pa_context* c, pa_context_state_t newState) {
        c->state.state = newState;

        if (c && c->callbacks.notify.cb) {
            c->callbacks.notify.cb(c, c->callbacks.notify.userdata);
        }
    }

}

pa_context *pa_context_new(pa_mainloop_api* m, const char* name) {
    PCM_STUB();

    auto context = static_cast<pa_context*>(pa_xmalloc(sizeof(pa_context)));
    std::construct_at(context);
    pa_context_ref(context);
    
    context->state.state = PA_CONTEXT_UNCONNECTED;
    context->state.error = PA_OK;
    context->state.refs = 0;

    context->callbacks.notify.cb = nullptr;
    context->callbacks.notify.userdata = nullptr;

    context->network.factory = laar::MessageFactory::configure();
    context->network.buffer = std::make_unique<std::uint8_t[]>(laar::NetworkBufferSize);
    context->network.size = laar::NetworkBufferSize;

    context->network.factory->withMessageVersion(laar::message::version::FIRST);

    // open & configure socket
    if ((context->network.fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        pcm_log::log(strerror(errno), pcm_log::ELogVerbosity::ERROR);
        pa_context_unref(context);
        return nullptr;
    }

    sockaddr_in addr;
    addr.sin_port = laar::Port;
    addr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) < 0) {
        pcm_log::log("failed to convert address to binary format", pcm_log::ELogVerbosity::ERROR);
        changeContextState(context, PA_CONTEXT_FAILED);

        pa_context_unref(context);
        return nullptr;
    }

    changeContextState(context, PA_CONTEXT_CONNECTING);

    if (connect(context->network.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        pcm_log::log(strerror(errno), pcm_log::ELogVerbosity::ERROR);
        changeContextState(context, PA_CONTEXT_FAILED);

        pa_context_unref(context);
        return nullptr;
    }

    changeContextState(context, PA_CONTEXT_AUTHORIZING);
    changeContextState(context, PA_CONTEXT_SETTING_NAME);

    std::size_t nameLen = strlen(name);
    char* data = new char[nameLen];
    std::memcpy(data, name, nameLen);



    return context;
}

void pa_context_unref(pa_context* c) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));
    
    c->state.refs -= 1;
    if (c->state.refs < 0) {
        // don't forget to cleanup fd
        if (close(c->network.fd) < 0) {
            changeContextState(c, PA_CONTEXT_FAILED);
        } else {
            if (!(c->state.state & PA_CONTEXT_FAILED)) {
                changeContextState(c, PA_CONTEXT_TERMINATED);
            }
        }

        std::destroy_at(c);
        pa_xfree(c);
    }
}

pa_context* pa_context_ref(pa_context* c) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(c), nullptr);

    c->state.refs += 1;
    return c;
}

void pa_context_set_state_callback(pa_context* c, pa_context_notify_cb_t cb, void* userdata) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));

    c->callbacks.notify.cb = cb;
    c->callbacks.notify.userdata = userdata;
}

int pa_context_errno(const pa_context *c) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(c), PA_ERR_EXIST);

    return c->state.error;
}

pa_context_state_t pa_context_get_state(const pa_context *c) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));

    return c->state.state;
}

pa_operation* pa_context_exit_daemon(pa_context *c, pa_context_success_cb_t cb, void *userdata) {
    PCM_MISSED_STUB();
    UNUSED(c);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_context_proplist_update(pa_context* c, pa_update_mode_t mode, const pa_proplist* p, pa_context_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(c);
    UNUSED(mode);
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_context_proplist_remove(pa_context* c, const char *const keys[], pa_context_success_cb_t cb, void* userdata) {
    PCM_MISSED_STUB();
    UNUSED(c);
    UNUSED(keys);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}