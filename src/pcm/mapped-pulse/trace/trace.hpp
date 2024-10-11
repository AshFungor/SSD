#pragma once

// pulse
#include <pulse/def.h>
#include <pulse/volume.h>

// Abseil
#include <absl/status/status.h>

// STD
#include <string>
#include <format>
#include <ostream>
#include <iostream>


namespace pcm_log {

    namespace __pcm_trace_internal {

        std::ostream& timedLog(std::ostream& os, const std::string& tag);
        std::ostream& error(std::ostream& os);
        std::ostream& info(std::ostream& os);
        std::ostream& warning(std::ostream& os);

    } // namespace __pcm_trace_internal

    // ensure sink exists for duration of PCM API usage
    absl::Status configureLogging(std::ostream* os);

    absl::Status logFormatError(const std::string& fmt, std::format_args args);
    absl::Status logFormatInfo(const std::string& fmt, std::format_args args);
    absl::Status logFormatWarning(const std::string& fmt, std::format_args args);

    void logErrorSilent(const std::string& fmt, std::format_args args);
    void logInfoSilent(const std::string& fmt, std::format_args args);
    void logWarningSilent(const std::string& fmt, std::format_args args);

    // pulse structs string converters
    std::string toString(const pa_buffer_attr *attr);
    std::string toString(const pa_cvolume *v);
    std::string toString(const pa_channel_position_t pos);
    std::string toString(const pa_channel_map *m);
    std::string toString(pa_sample_format_t sf);
    std::string toString(const pa_sample_spec *ss);

} // namespace pcm_log
