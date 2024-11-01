// pulse
#include <pulse/mainloop.h>
#include <pulse/mainloop-api.h>

// GTest
#include <gtest/gtest.h>

// standard
#include <cmath>
#include <iostream>

// laar
#include <src/ssd/macros.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

#define GTEST_COUT(chain) \
    std::cerr << "[INFO      ] " << chain << '\n';

namespace {

    constexpr int runsTotal = 3;

    void defer(pa_mainloop_api* api, pa_defer_event* e, void* userdata) {
        int* counter = reinterpret_cast<int*>(userdata);
        GTEST_COUT("executing deferred event: counter equals " << *counter);

        if (*counter >= runsTotal) {
            GTEST_COUT("execution has reached counter: " << *counter << "; disabling deferred event");
            api->defer_enable(e, 0);
            return;
        }

        (*counter)++;
    }

    void expired(pa_mainloop_api* a, pa_time_event* e, const struct timeval* tv, void* userdata) {
        UNUSED(a);
        UNUSED(e);
        UNUSED(tv);

        GTEST_COUT("timer called after " << tv->tv_sec << " seconds, " << tv->tv_usec << " nanoseconds");
        bool* called = reinterpret_cast<bool*>(userdata);
        *called = true;

        a->quit(a, PA_OK);
    }

    void destroyTimerCheck(pa_mainloop_api* api, pa_time_event* e, void* userdata) {
        UNUSED(e);
        UNUSED(api);

        bool* called = reinterpret_cast<bool*>(userdata);
        delete called;
    }

    void destroy(pa_mainloop_api* api, pa_defer_event* e, void* userdata) {
        UNUSED(e);
        UNUSED(api);

        int* casted = reinterpret_cast<int*>(userdata);
        delete casted;
    }

    class MainloopTest : public ::testing::Test {
    public:

        void SetUp() override {
            ENSURE_FAIL_UNLESS(pcm_log::configureLogging(&std::cerr).ok());
        }

    };

}

TEST_F(MainloopTest, TestDeferred) {
    pa_mainloop* m = pa_mainloop_new();
    ASSERT_TRUE(m);

    pa_mainloop_api* api = pa_mainloop_get_api(m);
    ASSERT_TRUE(api);

    int* counter = new int{0};
    pa_defer_event* event = api->defer_new(api, defer, counter);
    ASSERT_TRUE(event);

    api->defer_set_destroy(event, destroy);

    for (std::size_t i = 0; i < runsTotal; ++i) {
        GTEST_COUT("running mainloop iteration: " << i)
        pa_mainloop_iterate(m, 0, nullptr);
    }

    ASSERT_EQ(*counter, runsTotal);
    // run one time to ensure event was not triggered
    pa_mainloop_iterate(m, 0, nullptr);
    ASSERT_EQ(*counter, runsTotal);

    api->defer_free(event);
    pa_mainloop_free(m);
}

TEST_F(MainloopTest, TestTimer) {
    pa_mainloop* m = pa_mainloop_new();
    ASSERT_TRUE(m);

    pa_mainloop_api* api = pa_mainloop_get_api(m);
    ASSERT_TRUE(api);

    timeval time {
        .tv_sec = 1,
        .tv_usec = 0
    };

    bool* check = new bool{false};
    pa_time_event* event = api->time_new(api, &time, expired, check);

    api->time_set_destroy(event, destroyTimerCheck);

    pa_mainloop_prepare(m, 2 * 1000 * 1000); // 2 seconds = 2 microseconds * 10^6
    pa_mainloop_poll(m);
    pa_mainloop_dispatch(m);

    ASSERT_TRUE(*check);

    // this approach works too
    *check = false;

    api->time_restart(event, &time);
    pa_mainloop_run(m, nullptr);

    ASSERT_TRUE(*check);

    api->time_free(event);
    pa_mainloop_free(m);
}

TEST_F(MainloopTest, TestOnceCallback) {
    pa_mainloop* m = pa_mainloop_new();
    pa_mainloop_api* api = pa_mainloop_get_api(m);

    int* calledTimes = new int{0};
    auto onceCallback = [](pa_mainloop_api* a, void* data) {
        UNUSED(a);

        auto calledTimes = reinterpret_cast<int*>(data);
        ++(*calledTimes);
        GTEST_COUT("callback called times " << *calledTimes << " in total")

        if (*calledTimes >= 2) {
            a->quit(a, PA_OK);
        }
    };

    pa_mainloop_api_once(api, onceCallback, calledTimes);
    pa_mainloop_api_once(api, onceCallback, calledTimes);

    pa_mainloop_run(m, nullptr);

    ASSERT_EQ(*calledTimes, 2);
    delete calledTimes;

    pa_mainloop_free(m);
}