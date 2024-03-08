#pragma once
/* 
 * Macros for general purpose use.
 */

// standard
#include <cstdlib>
#include <thread>

#define SSD_ABORT_UNLESS(__condition)           \
    do {                                        \
        if (!__condition) {                     \
            std::abort();                       \
        }                                       \
    } while (false)

#define SSD_ABORT_UNLESS_SAME_THREAD(__single)                                  \
    do {                                                                        \
        SSD_ABORT_UNLESS(std::this_thread::get_id() != __single);               \
    } while (false)
