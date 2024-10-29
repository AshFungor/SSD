#pragma once

// STD
#include <cstdint>

// pulse
#include <pulse/gccmacro.h>

#define ENSURE_NOT_NULL(ptr)                                    \
    do {                                                        \
        if (ptr == nullptr) {                                   \
            throw std::runtime_error("pointer is null");        \
        }                                                       \
    } while (false)

#define UNUSED(variable) (void)(variable)

#define ENSURE_SIZE_IS_LESS_THAN_OR_EQUAL(size, target)                 \
    do {                                                                \
        if (size > target) {                                            \
            throw std::runtime_error("size of array is out of bounds"); \
        }                                                               \
    } while(false)

#define ENSURE_FAIL()                                                                                                                       \
    do {                                                                                                                                    \
        throw std::runtime_error(std::string{"fail requested, context: file: "} + __FILE__ + "; line: " + std::to_string(__LINE__));        \
    } while(false) 

#define ENSURE_FAIL_UNLESS(condition)                                   \
    do if (!(condition)) {                                              \
        ENSURE_FAIL();                                                  \
    }  while(false)     

namespace laar {

    inline constexpr int MaxBytesOnMessage = 2048;
    inline constexpr int NetworkBufferSize = 1024;
    inline constexpr int Port = 7777;
    inline constexpr int BaseSampleRate = 44100;
    inline constexpr std::int32_t Silence = INT32_MIN;
    inline constexpr std::int32_t BaseSampleSize = sizeof Silence;

}