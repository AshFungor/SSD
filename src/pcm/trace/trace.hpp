#pragma once

// pulse
#include <pulse/def.h>
#include <pulse/volume.h>

#include <ostream>
#include <string>
#include <format>

#if defined(LOG_TO_STD)
    #include <iostream>
    #include <mutex>
    // definitions for streams
    #define s_err std::cerr
    #define s_out std::cout
#elif defined(LOG_TO_FILE)
    // Implement logger
#else
    #error Logging method is not provided, pass either LOG_TO_STD or LOG_TO_FILE
#endif

namespace pcm_log {
    namespace __pcm_trace_internal {
        // global lock for standard error/output
        static std::mutex GStandardLock;
        std::ostream& timedLog(std::ostream& os, const std::string& tag);
        std::ostream& error(std::ostream& os);
        std::ostream& info(std::ostream& os);
        std::ostream& warning(std::ostream& os);
    } // namespace __pcm_trace_internal

    void logError(const std::string& fmt, std::format_args args);
    void logInfo(const std::string& fmt, std::format_args args);
    void logWarning(const std::string& fmt, std::format_args args);

    // pulse structs string converters
    std::string pa_buffer_attr_to_string(const pa_buffer_attr *attr);
    std::string pa_volume_to_string(const pa_cvolume *v);
    std::string pa_channel_position_t_to_string(const pa_channel_position_t pos);
    std::string pa_channel_map_to_string(const pa_channel_map *m);
    std::string pa_sample_format_t_to_string(pa_sample_format_t sf);
    std::string pa_sample_spec_to_string(const pa_sample_spec *ss);
} // namespace pcm_log
