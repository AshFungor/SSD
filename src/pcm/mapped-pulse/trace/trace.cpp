// Abseil (google common libs)
#include <absl/status/status.h>

// Pulse
#include <pulse/sample.h>

// local
#include <src/pcm/mapped-pulse/trace/trace.hpp>

// STD
#include <chrono>
#include <cstdint>
#include <ctime>
#include <format>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>


// case section for mapping enum values to string literals
#define CASE(literal, var, str) \
    case literal:               \
        var = str;              \
        break;

#define DEFAULT(var, str)       \
    default:                    \
        var = str;

using namespace pcm_log;

std::ostream& __pcm_trace_internal::timedLog(std::ostream& os, const std::string& tag) {
    const auto now = std::chrono::high_resolution_clock::now();
    const auto reps = now.time_since_epoch().count();
    os << std::put_time(std::localtime(&reps), "[%d:%m:%Y %H:%M:%S]");
    return os << "[PCM]" << tag << " ";
}

std::ostream& __pcm_trace_internal::error(std::ostream& os) {
    return __pcm_trace_internal::timedLog(os, "[error]");
}

std::ostream& __pcm_trace_internal::info(std::ostream& os) {
    return __pcm_trace_internal::timedLog(os, "[info]");
}

std::ostream& __pcm_trace_internal::warning(std::ostream& os) {
    return __pcm_trace_internal::timedLog(os, "[warning]");
}

absl::Status pcm_log::configureLogging(std::ostream* os) {
    __pcm_trace_internal::os = os;
    if (!os) {
        return absl::OkStatus();
    }
    return (os->bad()) ? absl::InternalError("Stream is invalid") : absl::OkStatus();
}

absl::Status pcm_log::logError(const std::string& fmt, std::format_args args) {
    std::unique_lock<std::mutex> lock {__pcm_trace_internal::GStandardLock};
    if (!__pcm_trace_internal::os) {
        return absl::OkStatus();
    }
    __pcm_trace_internal::error(*__pcm_trace_internal::os) << std::vformat(fmt, args);
    return (!__pcm_trace_internal::os->bad()) ? absl::OkStatus() : absl::InternalError("stream is unavailable for I/O");
}

absl::Status pcm_log::logInfo(const std::string& fmt, std::format_args args) {
    std::unique_lock<std::mutex> lock {__pcm_trace_internal::GStandardLock};
    if (!__pcm_trace_internal::os) {
        return absl::OkStatus();
    }
    __pcm_trace_internal::info(*__pcm_trace_internal::os) << std::vformat(fmt, args);
    return (!__pcm_trace_internal::os->bad()) ? absl::OkStatus() : absl::InternalError("stream is unavailable for I/O");
}

absl::Status pcm_log::logWarning(const std::string& fmt, std::format_args args) {
    std::unique_lock<std::mutex> lock {__pcm_trace_internal::GStandardLock};
    if (!__pcm_trace_internal::os) {
        return absl::OkStatus();
    }
    __pcm_trace_internal::warning(*__pcm_trace_internal::os) << std::vformat(fmt, args);
    return (!__pcm_trace_internal::os->bad()) ? absl::OkStatus() : absl::InternalError("stream is unavailable for I/O");
}

std::string pcm_log::toString(const pa_buffer_attr *attr) {
    if (!attr) {
        return "{null}";
    }
    std::stringstream ss;
    ss << "{maxlength = " << attr->maxlength << ", "
       << "tlength = " << attr->tlength << ", "
       << "prebuf = " << attr->prebuf << ", "
       << "minreq = " << attr->minreq << ", "
       << "fragsize = " << attr->fragsize << "}";
    return ss.str();
}

std::string pcm_log::toString(const pa_cvolume *v) {
    if (!v) {
        return "0:{}";
    }

    const auto channelCount = std::min<std::uint32_t>(v->channels, PA_CHANNELS_MAX);
    std::stringstream ss;

    ss << v->channels << ":{";
    for (std::size_t i = 0; i < channelCount; ++i) {
        if (i != 0) {
            ss << ", ";
        }
        ss << v->values[i];
    }
    ss << "}";

    return ss.str();
}

std::string pcm_log::toString(const pa_channel_position_t pos) {
    std::string result;

    switch (pos) {
        CASE(PA_CHANNEL_POSITION_INVALID, result, "INVALID")
        CASE(PA_CHANNEL_POSITION_MONO, result, "MONO")
        CASE(PA_CHANNEL_POSITION_FRONT_LEFT, result, "FRONT_LEFT")
        CASE(PA_CHANNEL_POSITION_FRONT_RIGHT, result, "FRONT_RIGHT")
        CASE(PA_CHANNEL_POSITION_FRONT_CENTER, result, "FRONT_CENTER")
        CASE(PA_CHANNEL_POSITION_REAR_CENTER, result, "REAR_CENTER")
        CASE(PA_CHANNEL_POSITION_REAR_LEFT, result, "REAR_LEFT")
        CASE(PA_CHANNEL_POSITION_REAR_RIGHT, result, "REAR_RIGHT")
        CASE(PA_CHANNEL_POSITION_LFE, result, "LFE")
        CASE(PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER, result, "FRONT_LEFT_OF_CENTER")
        CASE(PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER, result, "FRONT_RIGHT_OF_CENTER")
        CASE(PA_CHANNEL_POSITION_SIDE_LEFT, result, "SIDE_LEFT")
        CASE(PA_CHANNEL_POSITION_SIDE_RIGHT, result, "SIDE_RIGHT")
        CASE(PA_CHANNEL_POSITION_AUX0, result, "AUX0")
        CASE(PA_CHANNEL_POSITION_AUX1, result, "AUX1")
        CASE(PA_CHANNEL_POSITION_AUX2, result, "AUX2")
        CASE(PA_CHANNEL_POSITION_AUX3, result, "AUX3")
        CASE(PA_CHANNEL_POSITION_AUX4, result, "AUX4")
        CASE(PA_CHANNEL_POSITION_AUX5, result, "AUX5")
        CASE(PA_CHANNEL_POSITION_AUX6, result, "AUX6")
        CASE(PA_CHANNEL_POSITION_AUX7, result, "AUX7")
        CASE(PA_CHANNEL_POSITION_AUX8, result, "AUX8")
        CASE(PA_CHANNEL_POSITION_AUX9, result, "AUX9")
        CASE(PA_CHANNEL_POSITION_AUX10, result, "AUX10")
        CASE(PA_CHANNEL_POSITION_AUX11, result, "AUX11")
        CASE(PA_CHANNEL_POSITION_AUX12, result, "AUX12")
        CASE(PA_CHANNEL_POSITION_AUX13, result, "AUX13")
        CASE(PA_CHANNEL_POSITION_AUX14, result, "AUX14")
        CASE(PA_CHANNEL_POSITION_AUX15, result, "AUX15")
        CASE(PA_CHANNEL_POSITION_AUX16, result, "AUX16")
        CASE(PA_CHANNEL_POSITION_AUX17, result, "AUX17")
        CASE(PA_CHANNEL_POSITION_AUX18, result, "AUX18")
        CASE(PA_CHANNEL_POSITION_AUX19, result, "AUX19")
        CASE(PA_CHANNEL_POSITION_AUX20, result, "AUX20")
        CASE(PA_CHANNEL_POSITION_AUX21, result, "AUX21")
        CASE(PA_CHANNEL_POSITION_AUX22, result, "AUX22")
        CASE(PA_CHANNEL_POSITION_AUX23, result, "AUX23")
        CASE(PA_CHANNEL_POSITION_AUX24, result, "AUX24")
        CASE(PA_CHANNEL_POSITION_AUX25, result, "AUX25")
        CASE(PA_CHANNEL_POSITION_AUX26, result, "AUX26")
        CASE(PA_CHANNEL_POSITION_AUX27, result, "AUX27")
        CASE(PA_CHANNEL_POSITION_AUX28, result, "AUX28")
        CASE(PA_CHANNEL_POSITION_AUX29, result, "AUX29")
        CASE(PA_CHANNEL_POSITION_AUX30, result, "AUX30")
        CASE(PA_CHANNEL_POSITION_AUX31, result, "AUX31")
        CASE(PA_CHANNEL_POSITION_TOP_CENTER, result, "TOP_CENTER")
        CASE(PA_CHANNEL_POSITION_TOP_FRONT_LEFT, result, "TOP_FRONT_LEFT")
        CASE(PA_CHANNEL_POSITION_TOP_FRONT_RIGHT, result, "TOP_FRONT_RIGHT")
        CASE(PA_CHANNEL_POSITION_TOP_FRONT_CENTER, result, "TOP_FRONT_CENTER")
        CASE(PA_CHANNEL_POSITION_TOP_REAR_LEFT, result, "TOP_REAR_LEFT")
        CASE(PA_CHANNEL_POSITION_TOP_REAR_RIGHT, result, "TOP_REAR_RIGHT")
        CASE(PA_CHANNEL_POSITION_TOP_REAR_CENTER, result, "TOP_REAR_CENTER")
        CASE(PA_CHANNEL_POSITION_MAX, result, "MAX")
        DEFAULT(result, "UNKNOWN")
    }

    int posNumeric = pos;
    return std::vformat("{}({})", std::make_format_args(result, posNumeric));
}

std::string pcm_log::toString(const pa_channel_map *m) {
    if (!m) {
        return "{null}";
    }

    std::stringstream ss;
    ss << m->channels << ":{";

    const auto channel_count = std::min<std::uint32_t>(m->channels, PA_CHANNELS_MAX);
    for (std::size_t i = 0; i < channel_count; ++i) {
        if (i != 0) {
            ss << ", ";
        }
        ss << toString(m->map[i]);
    }
    ss << "}";

    return ss.str();
}

std::string pcm_log::toString(pa_sample_format_t sf) {
    std::string fmt;
    switch (sf) {
        CASE(PA_SAMPLE_U8, fmt, "U8")
        CASE(PA_SAMPLE_ALAW, fmt, "ALAW")
        CASE(PA_SAMPLE_ULAW, fmt, "ULAW")
        CASE(PA_SAMPLE_S16LE, fmt, "S16LE")
        CASE(PA_SAMPLE_S16BE, fmt, "S16BE")
        CASE(PA_SAMPLE_FLOAT32LE, fmt, "FLOAT32LE")
        CASE(PA_SAMPLE_FLOAT32BE, fmt, "FLOAT32BE")
        CASE(PA_SAMPLE_S32LE, fmt, "S32LE")
        CASE(PA_SAMPLE_S32BE, fmt, "S32BE")
        CASE(PA_SAMPLE_S24LE, fmt, "S24LE")
        CASE(PA_SAMPLE_S24BE, fmt, "S24BE")
        CASE(PA_SAMPLE_S24_32LE, fmt, "S24_32LE")
        CASE(PA_SAMPLE_S24_32BE, fmt, "S24_32BE")
        CASE(PA_SAMPLE_MAX, fmt, "MAX")
        CASE(PA_SAMPLE_INVALID, fmt, "INVALID")
        DEFAULT(fmt, "UNKNOWN")
    }

    int sfNumeric = sf;
    return std::vformat("{}({})", std::make_format_args(fmt, sfNumeric));
}

std::string pcm_log::toString(const pa_sample_spec *ss) {
    if (!ss) {
        return "{null}";
    }

    const auto format = toString(ss->format);
    return std::vformat("{format = {}, rate = {}, channels = {}}",
                        std::make_format_args(format, ss->rate, ss->channels));
}
