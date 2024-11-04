// pulse
#include <pulse/def.h>
#include <pulse/context.h>
#include <pulse/xmalloc.h>
#include <pulse/context.h>
#include <pulse/operation.h>
#include <pulse/mainloop-api.h>

// protos
#include <protos/holder.pb.h>
#include <protos/client-message.pb.h>
#include <protos/client/context.pb.h>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

// laar
#include <src/ssd/macros.hpp>
#include <src/ssd/core/message.hpp>
#include <src/pcm/mapped-pulse/context/common.hpp>

#ifdef __linux__
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

// STD
#include <cerrno>
#include <cstring>
#include <memory>

// abseil
#include <absl/strings/str_format.h>

#define HANDLE_SOCKET_ERROR(errnum, context)                                        \
    do {                                                                            \
        switch (errnum) {                                                           \
            case EAGAIN:                                                            \
                return;                                                             \
            default:                                                                \
                pcm_log::log(strerror(errnum), pcm_log::ELogVerbosity::ERROR);      \
                changeContextState(context, PA_CONTEXT_FAILED);                     \
                return;                                                             \
        }                                                                           \
    } while (false)

namespace {

    enum Mode {
        READ = 0x1, 
        WRITE = 0x2,
        DRAINING = 0x4
    };

    void logContextNetworkState(pa_context* c, const char* message) {
        pcm_log::log(absl::StrFormat("[context] %s; context: t = %d, c = %d, e = %d", message, c->network.total, c->network.current, c->network.expected), pcm_log::ELogVerbosity::INFO);
    }

    void changeContextState(pa_context* c, pa_context_state_t newState) {
        c->state.state = newState;

        if (c && c->callbacks.notify.cb) {
            c->callbacks.notify.cb(c, c->callbacks.notify.userdata);
        }
    }

    void iterate(pa_mainloop_api* a, pa_io_event* e, int fd, pa_io_event_flags flags, void* userdata) {
        pa_context* c = reinterpret_cast<pa_context*>(userdata);

        // context on error means that this object is not valid anymore
        if (c->state.error != PA_OK) {
            a->io_enable(e, PA_IO_EVENT_NULL);
            return;
        }

        if (c->network.mode & DRAINING && !(c->network.mode & ~DRAINING)) {
            // we got our cycle, just leave now
            if (c->callbacks.drained.cb) {
                c->callbacks.drained.cb(c, c->callbacks.drained.userdata);
                if (c->state.drain) {
                    laar::updateOp(c->state.drain, PA_OPERATION_DONE);
                    pa_operation_unref(c->state.drain);
                }
            }
            return;
        }

        if (flags & PA_IO_EVENT_OUTPUT && c->network.mode & WRITE) {
            if (!c->network.total) {
                for (auto iter = c->out.begin(); iter != c->out.end(); ++iter, ++c->network.expected) {
                    // check on buffer for one more message
                    if (c->network.total + laar::Message::Size::total(&iter->message) > laar::MaxBytesOnMessage - laar::StreamTrailSize) {
                        break;
                    }
                    // add it
                    logContextNetworkState(c, "adding message to stream");
                    iter->message.writeToArray(c->network.buffer.get() + c->network.total, c->network.size - c->network.total);
                    c->network.total += laar::Message::Size::total(&iter->message);
                    laar::updateOp(iter->op, PA_OPERATION_RUNNING);
                }
            }

            // skip write if all empty
            if (!c->network.expected) {
                return;
            }

            // add trailing message
            logContextNetworkState(c, "adding trail to stream");
            auto trail = c->network.factory->withType(laar::message::type::SIMPLE)
                .withPayload(laar::TRAIL)
                .construct()
                .constructed();
            trail.writeToArray(c->network.buffer.get() + c->network.total, c->network.size - c->network.total);
            c->network.total += laar::Message::Size::total(&trail);
            ++c->network.expected;

            while (c->network.current < c->network.total) {
                logContextNetworkState(c, "writing stream");
                if (int written = write(fd, c->network.buffer.get() + c->network.current, c->network.total - c->network.current); written >= 0) {
                    c->network.current += written;
                } else HANDLE_SOCKET_ERROR(written, c);
            }

            if (c->network.current == c->network.total) {
                c->network.current = c->network.total = 0;
                c->network.mode = READ | ((c->network.mode & DRAINING) ? DRAINING : 0);
            }
        }

        if (flags & PA_IO_EVENT_INPUT && c->network.mode & READ) {

            if (!c->network.expected) {
                return;
            }

            if (c->network.total == 0) {
                c->network.total += c->network.factory->next();
            }

            if (int acquired = read(fd, c->network.buffer.get() + c->network.current, c->network.total - c->network.current); acquired >= 0) {
                c->network.current += acquired;
            } else HANDLE_SOCKET_ERROR(acquired, c);

            while (c->network.current == c->network.total) {
                logContextNetworkState(c, "reading chunk");
                std::size_t bytes = c->network.size;
                c->network.total += c->network.factory->parse(c->network.buffer.get() + c->network.current - c->network.factory->next(), bytes);

                if (c->network.factory->isParsedAvailable()) {
                    logContextNetworkState(c, "getting message");

                    if (!--c->network.expected) {
                        logContextNetworkState(c, "last message on stream");
                        auto trail = c->network.factory->parsed();
                        if (trail.type() != laar::message::type::SIMPLE || laar::messagePayload<laar::message::type::SIMPLE>(trail) != laar::TRAIL) {
                            changeContextState(c, PA_CONTEXT_FAILED);
                            return;
                        }
                        c->network.current = c->network.total = 0;

                        // draining should be the only state for check to unfold
                        c->network.mode = ((c->network.mode & DRAINING) ? DRAINING : WRITE);
                        break;
                    }

                    auto queuer = std::move(c->out.front());
                    c->out.pop_front();
                    queuer.op->cbSuccess(c->network.factory->parsed(), queuer.op->owner);
                    laar::updateOp(queuer.op, PA_OPERATION_DONE);
                    pa_operation_unref(queuer.op);
                }

                if (int acquired = read(fd, c->network.buffer.get() + c->network.current, c->network.total - c->network.current); acquired >= 0) {
                    c->network.current += acquired;
                } else HANDLE_SOCKET_ERROR(acquired, c);
            }
        }
    }

    void queryContextConnection(pa_context* context, NSound::THolder holder, pa_operation* op) {
        auto message = context->network.factory->withType(laar::message::type::PROTOBUF)
            .withPayload(std::move(holder))
            .construct()
            .constructed();

        context->out.push_back(pa_context::QueuedMessage{
            .message = std::move(message),
            .op = op
        });
    }

    int failContextWithSyscall(pa_context* c) {
        pcm_log::log(strerror(errno), pcm_log::ELogVerbosity::ERROR);
        changeContextState(c, PA_CONTEXT_FAILED);
        pa_context_unref(c);

        return PA_ERR_PROTOCOL;
    }

    void contextNameConfirmed(laar::Message message, void* owner) {
        auto context = reinterpret_cast<pa_context*>(owner);
        ENSURE_FAIL_UNLESS(message.type() == laar::message::type::SIMPLE);
        std::uint32_t simple = laar::messagePayload<laar::message::type::SIMPLE>(message);
        if (simple == laar::ACK) {
            changeContextState(context, PA_CONTEXT_READY);
        } else {
            changeContextState(context, PA_CONTEXT_FAILED);
        }
    }

}

pa_context *pa_context_new(pa_mainloop_api* m, const char* name) {
    PCM_STUB();

    auto context = static_cast<pa_context*>(pa_xmalloc(sizeof(pa_context)));
    std::construct_at(context);

    context->state.drain = nullptr;

    context->state.name = name;   
    context->state.api = m; 
    context->state.state = PA_CONTEXT_UNCONNECTED;
    context->state.error = PA_OK;
    context->state.refs = 0;

    context->callbacks.notify.cb = nullptr;
    context->callbacks.notify.userdata = nullptr;
    context->callbacks.drained.cb = nullptr;
    context->callbacks.drained.userdata = nullptr;

    context->network.factory = laar::MessageFactory::configure();
    context->network.buffer = std::make_unique<std::uint8_t[]>(laar::NetworkBufferSize);
    context->network.size = laar::NetworkBufferSize;
    context->network.current = context->network.total = context->network.expected = 0;
    context->network.mode = WRITE;
    context->network.factory->withMessageVersion(laar::message::version::FIRST);

    context->events.iter = nullptr;

    // open & configure socket
    if ((context->network.fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        failContextWithSyscall(context);
        return nullptr;
    }

    return context;
}

pa_context *pa_context_new_with_proplist(pa_mainloop_api* mainloop, const char* name, const pa_proplist* proplist) {
    PCM_STUB();
    UNUSED(proplist);
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(mainloop));

    return pa_context_new(mainloop, name);
}

int pa_context_connect(pa_context *c, const char *server, pa_context_flags_t flags, const pa_spawn_api *api) {
    PCM_STUB();
    UNUSED(server);
    UNUSED(flags);
    UNUSED(api);

    // set blocking mode for configuration
    if (int err = fcntl(c->network.fd, F_SETFL, fcntl(c->network.fd, F_GETFL, 0) & ~O_NONBLOCK); err < 0) {
        return failContextWithSyscall(c);
    }

    sockaddr_in addr;
    addr.sin_port = laar::Port;
    addr.sin_family = AF_INET;
    
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) < 0) {
        return failContextWithSyscall(c);
    }

    changeContextState(c, PA_CONTEXT_CONNECTING);
    if (connect(c->network.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return failContextWithSyscall(c);;
    }

    changeContextState(c, PA_CONTEXT_AUTHORIZING);
    changeContextState(c, PA_CONTEXT_SETTING_NAME);

    c->events.iter = c->state.api->io_new(c->state.api, c->network.fd, static_cast<pa_io_event_flags_t>(PA_IO_EVENT_NULL), iterate, c);
    c->state.api->io_enable(c->events.iter, static_cast<pa_io_event_flags_t>(PA_IO_EVENT_INPUT | PA_IO_EVENT_OUTPUT));

    NSound::THolder holder;
    holder.mutable_client()->mutable_context_message()->mutable_connect()->set_name(c->state.name);

    auto op = new pa_operation;
    op->userdata = nullptr;
    op->cbNotify = nullptr;
    op->cbSuccess = contextNameConfirmed;
    op->owner = c;
    op->refs = 0;

    pa_operation_ref(op);

    queryContextConnection(c, std::move(holder), op);

    // set blocking mode back
    if (int err = fcntl(c->network.fd, F_SETFL, fcntl(c->network.fd, F_GETFL, 0) | O_NONBLOCK); err < 0) {
        return failContextWithSyscall(c);
    }

    return PA_OK;
}

void pa_context_unref(pa_context* c) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));
    
    c->state.refs -= 1;
    if (c->state.refs <= 0) {
        // don't forget to cleanup fd
        if (close(c->network.fd) < 0) {
            pcm_log::log(strerror(errno), pcm_log::ELogVerbosity::ERROR);
            changeContextState(c, PA_CONTEXT_FAILED);
        } else if (!(c->state.state & PA_CONTEXT_FAILED)) {
            changeContextState(c, PA_CONTEXT_TERMINATED);
        }

        if (c->events.iter) {
            c->state.api->io_free(c->events.iter);
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

void pa_context_set_event_callback(pa_context *p, pa_context_event_cb_t cb, void *userdata) {
    PCM_MISSED_STUB();
    UNUSED(p);
    UNUSED(cb);
    UNUSED(userdata);
}

int pa_context_is_pending(const pa_context *c) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(c), PA_ERR_EXIST);

    // TODO: this is very dependant on implementation,
    // should probably pick a more generic approach in the future
    return c->network.expected > 0;
}

void pa_context_disconnect(pa_context *c) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));

    // unref and set error to prevent IO from running
    c->state.error = PA_ERR_KILLED;
    pa_context_unref(c);
}

pa_operation* pa_context_drain(pa_context *c, pa_context_notify_cb_t cb, void *userdata) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));

    pa_operation* o = new pa_operation;
    o->refs = 0;
    o->state = PA_OPERATION_RUNNING;
    o->cbNotify = nullptr;
    o->cbSuccess = nullptr;
    o->userdata = nullptr;
    o->owner = nullptr;
    pa_operation_ref(o);

    c->callbacks.drained.cb = cb;
    c->callbacks.drained.userdata = userdata;
    c->network.mode |= DRAINING;

    c->state.drain = o;

    return o;
}

pa_operation* pa_context_set_default_sink(pa_context *c, const char *name, pa_context_success_cb_t cb, void *userdata) {
    PCM_MISSED_STUB();
    UNUSED(c);
    UNUSED(name);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

pa_operation* pa_context_set_default_source(pa_context *c, const char *name, pa_context_success_cb_t cb, void *userdata) {
    PCM_MISSED_STUB();
    UNUSED(c);
    UNUSED(name);
    UNUSED(cb);
    UNUSED(userdata);

    return nullptr;
}

int pa_context_is_local(const pa_context *c) {
    PCM_STUB();

    return pa_context_get_state(c) != PA_CONTEXT_UNCONNECTED;
}

pa_operation* pa_context_set_name(pa_context *c, const char *name, pa_context_success_cb_t cb, void *userdata) {
    PCM_MISSED_STUB();
    UNUSED(name);

    // simulate like we actually did some job since caller might
    // abort if nullptr is returned
    pa_operation* o = new pa_operation;
    o->refs = 0;
    o->cbNotify = nullptr;
    o->cbSuccess = nullptr;
    o->state = PA_OPERATION_DONE;
    o->userdata = userdata;
    pa_operation_ref(o);

    if (cb) {
        cb(c, 1, userdata);
    }

    pa_operation_unref(o);
    return o;
}

const char* pa_context_get_server(const pa_context *c) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));

    return "laar-server";
}

uint32_t pa_context_get_protocol_version(const pa_context *c) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));

    return PA_PROTOCOL_VERSION;
}

uint32_t pa_context_get_server_protocol_version(const pa_context *c) {
    PCM_PARTIAL_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));

    // does it even matter in general use, if introspec is not supported?
    return 0;
}

uint32_t pa_context_get_index(const pa_context *s) {
    PCM_PARTIAL_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(s));

    // does it even matter in general use, if introspec is not supported?
    return 0;
}

pa_time_event* pa_context_rttime_new(const pa_context *c, pa_usec_t usec, pa_time_event_cb_t cb, void *userdata) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));
    
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = usec;
    return c->state.api->time_new(c->state.api, &tv, cb, userdata);
}

void pa_context_rttime_restart(const pa_context *c, pa_time_event *e, pa_usec_t usec) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(e));

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = usec;
    c->state.api->time_restart(e, &tv);
}

size_t pa_context_get_tile_size(const pa_context *c, const pa_sample_spec *ss) {
    PCM_PARTIAL_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(c));
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(ss));

    // for generic use, around 200 samples per buffer is good enough
    return 200;
}

int pa_context_load_cookie_from_file(pa_context *c, const char *cookie_file_path) {
    PCM_MISSED_STUB();
    UNUSED(c);
    UNUSED(cookie_file_path);

    return PA_ERR_NOTSUPPORTED;
}
