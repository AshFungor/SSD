#pragma once

// STD
#include <cstdint>

// pulse
#include <pulse/gccmacro.h>

#ifdef __GNUC__
#define PCM_GCC_NORETURN __attribute__((noreturn))
#else
/** Macro for no-return functions */
#define PCM_GCC_NORETURN
#endif

// STUB for fully functional APIs
#define PCM_STUB()                                                          \
    pcm_log::log(absl::StrFormat(                                           \
        "stub PCM: %s:%d: %s", __FILE__, __LINE__, __PRETTY_FUNCTION__),    \
        pcm_log::ELogVerbosity::INFO                                        \
    )

// STUB for partially functional APIs, behaviors might vary
// or certain composition of arguments unsupported.
#define PCM_PARTIAL_STUB()                                                              \
    pcm_log::log(absl::StrFormat(                                                       \
        "stub PCM (partial): %s:%d: %s", __FILE__, __LINE__, __PRETTY_FUNCTION__),      \
        pcm_log::ELogVerbosity::WARNING                                                 \
    )

// STUB is not available for this API call.
#define PCM_MISSED_STUB()                                                               \
    pcm_log::log(absl::StrFormat(                                                       \
        "stub PCM (flatlined): %s:%d: %s", __FILE__, __LINE__, __PRETTY_FUNCTION__),    \
        pcm_log::ELogVerbosity::ERROR                                                   \
    )

#define PCM_MACRO_WRAPPER(macro, error_code)                        \
    try {                                                           \
        macro;                                                      \
    } catch (const std::exception& error) {                         \
        pcm_log::log(error.what(), pcm_log::ELogVerbosity::ERROR);  \
        return error_code;                                          \
    }

#define PCM_MACRO_WRAPPER_NO_RETURN(macro)                          \
    try {                                                           \
        macro;                                                      \
    } catch (const std::exception& error) {                         \
        pcm_log::log(error.what(), pcm_log::ELogVerbosity::ERROR);  \
    }

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