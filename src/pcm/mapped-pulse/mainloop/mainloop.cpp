// STD
#include <chrono>
#include <memory>

// pulse
#include <pulse/xmalloc.h>
#include <pulse/mainloop.h>
#include <pulse/mainloop-api.h>

// boost
#include <boost/bind/bind.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/detail/errc.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

// abseil
#include <absl/strings/str_format.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>
#include <src/pcm/mapped-pulse/mainloop/common.hpp>

using namespace std::chrono;

namespace {

    constexpr microseconds MainloopIterationTime {100};
    constexpr microseconds DeferEventsTriggerTime {MainloopIterationTime.count() + 1};

    struct once_info {
        void (*callback)(pa_mainloop_api*m, void *userdata);
        void *userdata;
    };

    enum EDeferredState {
        ON, OFF, REMOVED
    };

    void once_callback(pa_mainloop_api* m, pa_defer_event* e, void* userdata) {
        auto i = reinterpret_cast<once_info*>(userdata);

        if (i->callback) {
            i->callback(m, i->userdata);
        }

        if (m->defer_free) {
            m->defer_free(e);
        }
    }

    void free_callback(pa_mainloop_api *m, pa_defer_event *e, void *userdata) {
        UNUSED(m);
        UNUSED(e);

        auto i = reinterpret_cast<once_info*>(userdata);
        pa_xfree(i);
    }

    pa_io_event* io_new(pa_mainloop_api* a, int fd, pa_io_event_flags_t events, pa_io_event_cb_t callback, void* userdata) {
        PCM_MISSED_STUB();
        UNUSED(events);
        UNUSED(callback);
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(a));

        auto mainloop = reinterpret_cast<pa_mainloop*>(a->userdata);
        auto io_event = reinterpret_cast<pa_io_event*>(pa_xmalloc(sizeof(pa_io_event)));
        std::construct_at(io_event);

        io_event->state.api = a;
        io_event->state.cbDestroy = nullptr;
        io_event->state.userdata = userdata;
        io_event->stream = std::make_unique<boost::asio::posix::stream_descriptor>(*mainloop->impl->context, fd);

        // impl?

        return io_event;
    }

    void io_enable(pa_io_event* e, pa_io_event_flags_t events) {
        PCM_MISSED_STUB();
        UNUSED(events);
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(e));

        // do nothing for now
    }

    void io_free(pa_io_event* e) {
        PCM_STUB();

        if (e && e->state.cbDestroy) {
            e->state.cbDestroy(e->state.api, e, e->state.userdata);
        }

        pa_xfree(e);
    }

    void io_set_destroy(pa_io_event* e, pa_io_event_destroy_cb_t cb) {
        PCM_STUB();
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(e));

        e->state.cbDestroy = cb;
    }

    pa_time_event* time_new(pa_mainloop_api* a, const struct timeval* tv, pa_time_event_cb_t cb, void* userdata) {
        PCM_STUB();
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(a));
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(tv));

        auto mainloop = reinterpret_cast<pa_mainloop*>(a->userdata);
        auto timer = reinterpret_cast<pa_time_event*>(pa_xmalloc(sizeof(pa_time_event)));
        std::construct_at(timer);

        timer->timer = std::make_unique<boost::asio::steady_timer>(*mainloop->impl->context);
        timer->state.api = a;
        timer->state.cbComplete = cb;
        timer->state.cbDestroy = nullptr;
        timer->state.userdata = userdata;

        timer->timer->expires_after(std::chrono::seconds(tv->tv_sec) + std::chrono::microseconds(tv->tv_usec));
        timer->timer->async_wait([timer, tv](const boost::system::error_code& error) {
            if (!error && timer->state.cbComplete) {
                timer->state.cbComplete(timer->state.api, timer, tv, timer->state.userdata);
            }
        });

        return timer;
    }

    void time_restart(pa_time_event* e, const struct timeval *tv) {
        PCM_STUB();
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(e));

        if (std::chrono::steady_clock::now() < e->timer->expiry()) {
            e->timer->cancel();
        }

        e->timer->expires_after(std::chrono::seconds(tv->tv_sec) + std::chrono::microseconds(tv->tv_usec));
        e->timer->async_wait([e, tv](const boost::system::error_code& error) {
            if (!error && e->state.cbComplete) {
                e->state.cbComplete(e->state.api, e, tv, e->state.userdata);
            }
        });
    }
    
    void time_free(pa_time_event* e) {
        PCM_STUB();
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(e));

        if (std::chrono::steady_clock::now() < e->timer->expiry()) {
            e->timer->cancel();
        }

        if (e->state.cbDestroy) {
            e->state.cbDestroy(e->state.api, e, e->state.userdata);
        }

        pa_xfree(e);
    }

    void time_set_destroy(pa_time_event* e, pa_time_event_destroy_cb_t cb) {
        PCM_STUB();
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(e));

        e->state.cbDestroy = cb;
    }

    pa_defer_event* defer_new(pa_mainloop_api* a, pa_defer_event_cb_t cb, void* userdata) {
        PCM_PARTIAL_STUB();
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(a));

        auto mainloop = reinterpret_cast<pa_mainloop*>(a->userdata);
        auto defer = reinterpret_cast<pa_defer_event*>(pa_xmalloc(sizeof(pa_defer_event)));
        std::construct_at(defer);

        defer->state.b = new int{ON};
        defer->state.api = a;
        defer->state.userdata = userdata;
        defer->state.cbDestroy = nullptr;
        defer->state.cbDefer = cb;

        defer->timer = std::make_unique<boost::asio::steady_timer>(*mainloop->impl->context);
        defer->functor = [defer, b = defer->state.b](const boost::system::error_code& error) {
            if (*b == REMOVED || error) {
                delete b;
                return;
            }

            // defer might be cleared inside callback, so preserve any data we need now
            auto timer = std::move(defer->timer);
            auto functor = std::move(defer->functor);
            if (*b == ON && defer->state.cbDefer) {
                defer->state.cbDefer(defer->state.api, defer, defer->state.userdata);
            }

            // TODO: fix issue with deferred events overhead: when too many
            // events are registered and they take most of single iteration time, 
            // other stuff (timers, I/O) might perform poorly.
            timer->expires_after(DeferEventsTriggerTime);
            timer->async_wait(functor);

            // return data back to state
            if (*b != REMOVED) {
                defer->timer = std::move(timer);
                defer->functor = std::move(functor);
            }
        };

        boost::asio::defer(*mainloop->impl->context, boost::bind(defer->functor, boost::system::error_code{}));
        return defer;
    }

    void defer_enable(pa_defer_event* e, int b) {
        PCM_STUB();
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(e));

        *e->state.b = (b) ? ON : OFF;
    }

    void defer_free(pa_defer_event* e) {
        PCM_STUB();
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(e));

        if (e->state.cbDestroy) {
            e->state.cbDestroy(e->state.api, e, e->state.userdata);
        }

        *e->state.b = REMOVED;
        pa_xfree(e);
    }

    void defer_set_destroy(pa_defer_event *e, pa_defer_event_destroy_cb_t cb) {
        PCM_STUB();
        PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(e));

        e->state.cbDestroy = cb;
    }

    void quit(pa_mainloop_api* a, int retval) {
        PCM_STUB();

        auto mainloop = reinterpret_cast<pa_mainloop*>(a->userdata);
        pa_mainloop_quit(mainloop, retval);
    }

    void initMainloopApi(pa_mainloop* m) {
        m->impl->api = std::make_unique<pa_mainloop_api>();
        m->impl->api->userdata = m;

        // init callbacks here, try to make as much as possible

        // this is bullshit, I dunno how to support that
        m->impl->api->io_new = io_new;
        m->impl->api->io_free = io_free;
        m->impl->api->io_enable = io_enable;
        m->impl->api->io_set_destroy = io_set_destroy;

        // full support
        m->impl->api->time_new = time_new;
        m->impl->api->time_restart = time_restart;
        m->impl->api->time_set_destroy = time_set_destroy;
        m->impl->api->time_free = time_free;

        // partial support, have some issues with implementation
        m->impl->api->defer_new = defer_new;
        m->impl->api->defer_free = defer_free;
        m->impl->api->defer_enable = defer_enable;
        m->impl->api->defer_set_destroy = defer_set_destroy;

        m->impl->api->quit = quit;
    }

}

pa_mainloop *pa_mainloop_new(void) {
    PCM_STUB();

    pa_mainloop* mainloop = static_cast<pa_mainloop*>(pa_xmalloc(sizeof(pa_mainloop)));
    std::construct_at(mainloop);

    mainloop->impl = new struct mainloop;
    mainloop->impl->threading = nullptr;

    initMainloopApi(mainloop);

    mainloop->impl->context = std::make_unique<boost::asio::io_context>();
    return mainloop;
}

void pa_mainloop_free(pa_mainloop* m) {
    PCM_STUB();

    delete m->impl;
    pa_xfree(m);
}

int pa_mainloop_prepare(pa_mainloop *m, int timeout) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    if (m->impl->context->stopped()) {
        m->impl->context->restart();
    }

    m->impl->state.timeout = timeout;
    return PA_OK;
}

int pa_mainloop_poll(pa_mainloop *m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    if (m->impl->state.timeout < 0) {
        // run until quit
        auto guard = boost::asio::make_work_guard(*m->impl->context);
        m->impl->state.dispatched = m->impl->context->run();
    } else {
        auto guard = boost::asio::make_work_guard(*m->impl->context);
        m->impl->state.dispatched = m->impl->context->run_for(std::max(MainloopIterationTime, microseconds{m->impl->state.timeout}));
    }   

    m->impl->state.timeout = 0;
    return m->impl->state.retval;
}

int pa_mainloop_dispatch(pa_mainloop *m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    // poll already dispatches things, no need for this
    int dispatched = m->impl->state.dispatched;
    m->impl->state.dispatched = 0;
    return dispatched;
}

int pa_mainloop_get_retval(const pa_mainloop *m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    return m->impl->state.retval;
}

int pa_mainloop_iterate(pa_mainloop *m, int block, int *retval) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    pa_mainloop_prepare(m, (block) ? -1 : 0);
    pa_mainloop_poll(m);
    pa_mainloop_dispatch(m);

    if (retval) {
        *retval = m->impl->state.retval;
    }

    return m->impl->state.dispatched;
}

int pa_mainloop_run(pa_mainloop *m, int *retval) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    pa_mainloop_iterate(m, true, retval);

    // TODO: this should handle errors
    return PA_OK;
}

void pa_mainloop_quit(pa_mainloop *m, int retval) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    m->impl->state.retval = retval;
    m->impl->context->stop();
}

void pa_mainloop_wakeup(pa_mainloop *m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    m->impl->context->stop();
}

pa_mainloop_api* pa_mainloop_get_api(pa_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    return m->impl->api.get();
}

void pa_mainloop_set_poll_func(pa_mainloop *m, pa_poll_func poll_func, void *userdata) {
    PCM_STUB();
    UNUSED(m);
    UNUSED(userdata);
    UNUSED(poll_func);
}

void pa_mainloop_api_once(pa_mainloop_api* m, void (*callback)(pa_mainloop_api *m, void *userdata), void *userdata) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(callback));

    auto i = new once_info;
    i->callback = callback;
    i->userdata = userdata;

    auto e = m->defer_new(m, once_callback, i);
    m->defer_set_destroy(e, free_callback);
}
