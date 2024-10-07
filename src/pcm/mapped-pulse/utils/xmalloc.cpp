// Abseil
#include <absl/status/status.h>

// STD
#include <format>
#include <cerrno>
#include <cassert>
#include <csignal>

// pulseaudio
#include <pulse/xmalloc.h>

// local
#include <src/pcm/mapped-pulse/trace/trace.hpp>
#include <src/pcm/mapped-pulse/utils/macros.hpp>

// memory allocation provided by pulse.

namespace {

    /* 96 MB */
    static constexpr std::size_t MaxAllocationSize = 1024 * 1024 * 96;

    /* out-of-memory fallback */
    PCM_GCC_NORETURN void oom() {
        static const char error[] = "Not enough memory";
        pcm_log::logError("{}", std::make_format_args(error)).IgnoreError();
        std::abort();
    }

}

void* pa_xmalloc(std::size_t size) {
    void* p;
    assert(size > 0 && size < MaxAllocationSize);

    if (!(p = std::malloc(size))) {
        oom();
    }

    return p;
}

void* pa_xmalloc0(size_t size) {
    void* p;
    assert(size > 0 && size < MaxAllocationSize);

    if (!(p = std::calloc(1, size))) {
        oom();
    }

    return p;
}

void* pa_xrealloc(void *ptr, size_t size) {
    void* p;
    assert(size > 0 && size < MaxAllocationSize);

    if (!(p = std::realloc(ptr, size))) {
        oom();
    }

    return p;
}

void* pa_xmemdup(const void *p, size_t l) {
    if (!p) {
        return nullptr;
    }
    char *r = static_cast<char*>(pa_xmalloc(l));
    std::memcpy(r, p, l);
    return r;
}

char* pa_xstrdup(const char *s) {
    if (!s) {
        return nullptr;
    }

    return static_cast<char*>(pa_xmemdup(s, std::strlen(s) + 1));
}

char* pa_xstrndup(const char *s, size_t l) {
    char* r;
    const char* e;

    if (!s) {
        return nullptr;
    }

    if ((e = static_cast<const char*>(std::memchr(s, 0, l)))) {
        return static_cast<char*>(pa_xmemdup(s, static_cast<size_t>(e - s + 1)));
    }

    r = static_cast<char*>(pa_xmalloc(l + 1));
    std::memcpy(r, s, l);
    r[l] = 0;
    return r;
}

void pa_xfree(void *p) {
    int error;

    if (!p) {
        return;
    }

    error = errno;
    free(p);
    errno = error;
}