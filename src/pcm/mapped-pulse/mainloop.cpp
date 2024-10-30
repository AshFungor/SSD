// pulse
#include "pulse/def.h"
#include "pulse/mainloop-api.h"
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <pulse/mainloop.h>
#include <pulse/xmalloc.h>

// abseil
#include <absl/strings/str_format.h>

// laar
#include <src/ssd/macros.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

using namespace std::chrono;

struct pa_mainloop {
    std::unique_ptr<boost::asio::io_context> context;
    std::unique_ptr<pa_mainloop_api> api;
    
    struct State {
        int timeout = 0;
        int retval = PA_OK;
        int dispatched = 0;
    } state;
};

namespace {

    void initMainloopApi(pa_mainloop* m) {
        m->api = std::make_unique<pa_mainloop_api>();
        m->api->userdata = m;

        // init callbacks here, try to make as much as possibe
    }

}

pa_mainloop *pa_mainloop_new(void) {
    PCM_STUB();

    pa_mainloop* mainloop = static_cast<pa_mainloop*>(pa_xmalloc(sizeof(pa_mainloop)));
    std::construct_at(mainloop);

    initMainloopApi(mainloop);

    mainloop->context = std::make_unique<boost::asio::io_context>();
    return mainloop;
}

void pa_mainloop_free(pa_mainloop* m) {
    PCM_STUB();

    pa_mainloop_quit(m, PA_OK);
    pa_xfree(m);
}

int pa_mainloop_prepare(pa_mainloop *m, int timeout) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    if (m->context->stopped()) {
        m->context->restart();
    }

    m->state.timeout = timeout;
    return PA_OK;
}

int pa_mainloop_poll(pa_mainloop *m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    if (m->state.timeout < 0) {
        // run until quited
        auto guard = boost::asio::make_work_guard(*m->context);
        m->state.dispatched = m->context->run();
    } else if (m->state.timeout > 0) {
        // run only ready handlers
        m->state.dispatched = m->context->poll();
    } else {
        // run for a duration
        m->state.dispatched = m->context->run_for(milliseconds{m->state.timeout});
    }   

    m->state.timeout = 0;
    return m->state.retval;
}

int pa_mainloop_dispatch(pa_mainloop *m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    // poll already dispatches things, no need fot this
    int dispatched = m->state.dispatched;
    m->state.dispatched = 0;
    return dispatched;
}

int pa_mainloop_get_retval(const pa_mainloop *m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    return m->state.retval;
}

int pa_mainloop_iterate(pa_mainloop *m, int block, int *retval) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    pa_mainloop_prepare(m, (block) ? -1 : 0);
    pa_mainloop_poll(m);
    pa_mainloop_dispatch(m);

    if (retval) {
        *retval = m->state.retval;
    }

    return m->state.dispatched;
}

int pa_mainloop_run(pa_mainloop *m, int *retval) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    pa_mainloop_iterate(m, true, retval);
    return m->state.retval;
}

void pa_mainloop_quit(pa_mainloop *m, int retval) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    m->state.retval = retval;
    m->context->stop();
}

void pa_mainloop_wakeup(pa_mainloop *m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m);, PA_ERR_EXIST);

    m->context->stop();
}

void pa_mainloop_set_poll_func(pa_mainloop *m, pa_poll_func poll_func, void *userdata) {
    PCM_STUB();
    UNUSED(m);
    UNUSED(userdata);
    UNUSED(poll_func);
}








