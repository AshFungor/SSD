// pulse
#include <pulse/util.h>

// STD
#include <string>
#include <chrono>
#include <thread>
#include <cstdlib>

// laar
#include <src/ssd/macros.hpp>
#include <src/pcm/mapped-pulse/trace/trace.hpp>

#ifdef __linux__
#include <unistd.h>
#endif


char* pa_get_user_name(char* s, size_t l) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(s), nullptr);

    std::string name = std::getenv("USER");
    if (l < name.size()) {
        return nullptr;
    }

    std::memcpy(s, name.c_str(), name.size());
    std::memset(s + name.size(), 0, l - name.size());

    return s;
}

char* pa_get_host_name(char* s, size_t l) {
#ifdef __linux__

    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(s), nullptr);

    if (gethostname(s, l) < 0) {
        return nullptr;
    }

    return s;
    
#else
    PCM_MISSED_STUB();
#endif
}

char *pa_get_fqdn(char *s, size_t l) {
    PCM_MISSED_STUB();
    UNUSED(s);
    UNUSED(l);

    // I'm lazy
    return nullptr;
}


char* pa_get_home_dir(char* s, size_t l) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(s), nullptr);

    std::string dir = std::getenv("HOME");
    if (l < dir.size()) {
        return nullptr;
    }

    std::memcpy(s, dir.c_str(), dir.size());
    std::memset(s + dir.size(), 0, l - dir.size());

    return s;
}

char* pa_get_binary_name(char *s, size_t l) {
    PCM_STUB();
    PCM_MACRO_WRAPPER(ENSURE_NOT_NULL(s), nullptr);
    
    if (int retval = readlink("/proc/self/exe", s, l - 1); retval < 0) {
        return nullptr;
    }
    s[l - 1] = '\0';

    return s;
}

char* pa_path_get_filename(const char* p) {
    PCM_STUB();

    if (!p) {
        return nullptr;
    }

    char* fn;
    if ((fn = const_cast<char*>(strrchr(p, '\\')))) {
        return fn + 1;
    }

    return const_cast<char*>(p);
}

int pa_msleep(unsigned long t) {
    PCM_STUB();
    std::this_thread::sleep_for(std::chrono::milliseconds{t});
    return PA_OK;
}

int pa_thread_make_realtime(int rtprio) {
    PCM_MISSED_STUB();
    UNUSED(rtprio);

    return PA_ERR_NOTSUPPORTED;
}