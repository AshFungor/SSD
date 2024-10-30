// GTest
#include "pulse/mainloop-api.h"
#include "src/ssd/macros.hpp"
#include <gtest/gtest.h>

// standard
#include <cmath>

// laar
#include <pulse/mainloop.h>

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

    void destroy(pa_mainloop_api* api, pa_defer_event* e, void* userdata) {
        UNUSED(e);
        UNUSED(api);

        int* casted = reinterpret_cast<int*>(userdata);
        delete casted;
    }

}

TEST(MainloopTest, TestDeferred) {
    pa_mainloop* m = pa_mainloop_new();
    ASSERT_TRUE(m);

    pa_mainloop_api* api = pa_mainloop_get_api(m);
    ASSERT_TRUE(api);

    int* counter = new int{0};
    pa_defer_event* event = api->defer_new(api, defer, counter);
    ASSERT_TRUE(event);

    api->defer_set_destroy(event, destroy);

    for (std::size_t i = 0; i < runsTotal; ++i) {
        pa_mainloop_iterate(m, 0, nullptr);
    }

    ASSERT_EQ(*counter, runsTotal);
    // run one time to ensure event was not triggered
    pa_mainloop_iterate(m, 0, nullptr);
    ASSERT_EQ(*counter, runsTotal);

    api->defer_free(event);
    pa_mainloop_free(m);
}