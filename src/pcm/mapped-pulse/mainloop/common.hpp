#pragma once

// STD
#include <mutex>
#include <thread>
#include <memory>
#include <condition_variable>

// pulse
#include <pulse/def.h>
#include <pulse/mainloop.h>
#include <pulse/mainloop-api.h>
#include <pulse/thread-mainloop.h>

// boost
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

// abseil
#include <absl/strings/str_format.h>

// structure defs for threaded and simple mainloop.
// to ensure access from context, stream etc. to boost internals,
// only usage of mainloop is supported and mainloop_api's that hold
// data obtained from pa_mainloop_new() or pa_threaded_mainloop_new()
// will be valid in PCM.

// above is not relevant anymore :)

struct mainloop {

    struct Threading {
        bool abort;
        std::recursive_mutex mutex;
        std::condition_variable_any cv;
        std::unique_ptr<std::thread> thread;
    };

    // optional part with internals for threaded version
    std::unique_ptr<Threading> threading;

    // common members for loops
    std::unique_ptr<boost::asio::io_context> context;
    std::unique_ptr<pa_mainloop_api> api;
    
    // state for loop functional verbosity
    struct State {
        int timeout = 0;
        int retval = PA_OK;
        int dispatched = 0;
    } state;
};

// To ensure correct representation both loops hold just
// one pointer to actual object state.

struct pa_threaded_mainloop {
    mainloop* impl;
};

struct pa_mainloop {
    mainloop* impl;
};

struct pa_io_event {
    pa_defer_event* trigger;

    struct State {
        pa_io_event_destroy_cb_t cbDestroy;
        pa_io_event_cb_t cbNotify;
        pa_io_event_flags_t flags;
        pa_mainloop_api* api;
        void* userdata;
        int fd;
    } state;
};

struct pa_time_event {
    std::unique_ptr<boost::asio::steady_timer> timer;

    struct State {
        pa_time_event_destroy_cb_t cbDestroy;
        pa_time_event_cb_t cbComplete;
        pa_mainloop_api* api;
        void* userdata;
    } state;
};

struct pa_defer_event {
    std::function<void(const boost::system::error_code&)> functor;
    std::unique_ptr<boost::asio::steady_timer> timer;

    struct State {
        pa_defer_event_destroy_cb_t cbDestroy;
        pa_defer_event_cb_t cbDefer;
        pa_mainloop_api* api;
        void* userdata;
        int* b;
        int* defer_check;
    } state;
};