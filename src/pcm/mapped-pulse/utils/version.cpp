#include <pulse/version.h>

const char* pa_get_library_version(void) {
    return pa_get_headers_version();
}