#pragma once

#ifdef __GNUC__
#define PCM_GCC_NORETURN __attribute__((noreturn))
#else
/** Macro for no-return functions */
#define PCM_GCC_NORETURN
#endif