#pragma once

#include <cstdint>

#include <pulse/gccmacro.h>

namespace laar {

    inline constexpr int port = 7777;
    inline constexpr int BaseSampleRate = 44100;
    inline constexpr std::int32_t Silence = INT32_MIN;
    inline constexpr std::int32_t BaseSampleSize = sizeof Silence;

}