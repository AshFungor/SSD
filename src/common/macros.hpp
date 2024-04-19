#pragma once
/* 
 * Macros for general purpose use.
 */

// standard
#include <cstdlib>

template<typename Dummy>
void dummy(const Dummy& variable) {}

#define SSD_THROW_UNLESS(exception, message, condition)     \
    do {                                                    \
        if (!condition) {                                   \
            throw exception(message);                       \
        }                                                   \
    } while (false);

#define SSD_ABORT_UNLESS(condition)             \
    do {                                        \
        if (!condition) {                       \
            std::abort();                       \
        }                                       \
    } while (false)

#define SSD_ENSURE_THREAD(callbackQueue)                        \
    do {                                                        \
        SSD_ABORT_UNLESS(callbackQueue->isWorkerThread());      \
    } while (false)

#define SSD_UNUSED(variable)    \
    dummy(variable)             
