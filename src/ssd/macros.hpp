#pragma once

#include <cstdint>

#include <pulse/gccmacro.h>

namespace laar {

    inline constexpr int MaxBytesOnMessage = 2048;
    inline constexpr int NetworkBufferSize = 1024;
    inline constexpr int Port = 7777;
    inline constexpr int BaseSampleRate = 44100;
    inline constexpr std::int32_t Silence = INT32_MIN;
    inline constexpr std::int32_t BaseSampleSize = sizeof Silence;

}