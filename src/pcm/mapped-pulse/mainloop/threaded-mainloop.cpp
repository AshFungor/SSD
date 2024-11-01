// STD
#include <mutex>
#include <thread>
#include <memory>

// pulse
#include <pulse/xmalloc.h>
#include <pulse/mainloop.h>
#include <pulse/mainloop-api.h>
#include <pulse/thread-mainloop.h>

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

namespace {

}

pa_threaded_mainloop *pa_threaded_mainloop_new(void) {
    PCM_STUB();
    // threaded mainloop just needs some additional
    // functionality, all main things should be done by
    // common mainloop.
    auto mainloop = pa_mainloop_new();
    auto impl = mainloop->impl;
    mainloop->impl = nullptr;
    pa_mainloop_free(mainloop);

    impl->threading = std::make_unique<mainloop::Threading>();
    impl->threading->abort = false;
    auto m = static_cast<pa_threaded_mainloop*>(pa_xmalloc(sizeof(pa_threaded_mainloop)));
    std::construct_at(m);
    m->impl = impl;

    return m;
}

void pa_threaded_mainloop_free(pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    // should stop it first (request by API specs)
    pa_threaded_mainloop_stop(m);

    delete m->impl;
    pa_xfree(m);
}

int pa_threaded_mainloop_start(pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(m), PA_ERR_EXIST);

    if (m->impl->threading->thread) {
        ENSURE_FAIL();
    }

    m->impl->threading->thread = std::make_unique<std::thread>([m]() {
        while (!m->impl->threading->abort) {
            pa_threaded_mainloop_lock(m);
            pa_mainloop_iterate(reinterpret_cast<pa_mainloop*>(m), 0, nullptr);
            pa_threaded_mainloop_unlock(m);
            // make sure caller can lock thread here to perform tasks on mainloop's objects
        }
    });

    return PA_OK;
}

void pa_threaded_mainloop_stop(pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    // thread might not be started yet, then there is virtually nothing to stop
    if (!m->impl->threading->thread) {
        return;
    }

    pa_threaded_mainloop_lock(m);
    m->impl->threading->abort = true;
    pa_threaded_mainloop_unlock(m);
    if (m->impl->threading->thread->joinable()) {
        m->impl->threading->thread->join();
    }
}

void pa_threaded_mainloop_lock(pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    m->impl->threading->mutex.lock();
}

void pa_threaded_mainloop_unlock(pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    m->impl->threading->mutex.unlock();
}

void pa_threaded_mainloop_wait(pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    // it just seems that we wait for any signal to wakeup
    // it is said that we required to lock mutex with public API, but actually
    // nothing bad happens since mutex is recursive
    std::unique_lock<std::recursive_mutex> locked {m->impl->threading->mutex};
    m->impl->threading->cv.wait(locked);
}

void pa_threaded_mainloop_signal(pa_threaded_mainloop* m, int wait_for_accept) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    m->impl->threading->cv.notify_all();
    if (wait_for_accept) {
        std::unique_lock<std::recursive_mutex> locked {m->impl->threading->mutex};
        m->impl->threading->cv.wait(locked);
    }
}

void pa_threaded_mainloop_accept(pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    m->impl->threading->cv.notify_all();
}

int pa_threaded_mainloop_get_retval(const pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    return m->impl->state.retval;
}

pa_mainloop_api* pa_threaded_mainloop_get_api(pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    return m->impl->api.get();
}

int pa_threaded_mainloop_in_thread(pa_threaded_mainloop* m) {
    PCM_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    return std::this_thread::get_id() == m->impl->threading->thread->get_id();
}

void pa_threaded_mainloop_set_name(pa_threaded_mainloop* m, const char* name) {
    PCM_MISSED_STUB();
    PCM_MACRO_WRAPPER_NO_RETURN(ENSURE_NOT_NULL(m));

    UNUSED(m);
    UNUSED(name);
}

void pa_threaded_mainloop_once_unlocked(pa_threaded_mainloop* m, void (*callback)(pa_threaded_mainloop* m, void* userdata), void* userdata) {
    // most likely this is not supported by target platform anyway
    PCM_MISSED_STUB();

    UNUSED(m);
    UNUSED(callback);
    UNUSED(userdata);
}