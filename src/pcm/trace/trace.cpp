// STD
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>

#include "trace.hpp"

using namespace pcm_log;

std::ostream& __pcm_trace_internal::timedLog(std::ostream& os, const std::string& tag) {
    const auto now = std::chrono::high_resolution_clock::now();
    const auto reps = now.time_since_epoch().count();
    os << std::put_time(std::localtime(&reps), "[%d:%m:%Y %H:%M:%S]");
    return os << tag << " ";
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

void logError(const std::string& fmt, std::format_args args) {
    #if defined(LOG_LEVEL) && LOG_LEVEL <= 3
    std::scoped_lock<std::mutex> lock {__pcm_trace_internal::GStandardLock}; 
    __pcm_trace_internal::error(s_err) << std::vformat(fmt, args);
    __pcm_trace_internal::error(s_out) << std::vformat(fmt, args);
    #endif
}
void logInfo(const std::string& fmt, std::format_args args) {
    #if defined(LOG_LEVEL) && LOG_LEVEL <= 1
    std::scoped_lock<std::mutex> lock {__pcm_trace_internal::GStandardLock};
    __pcm_trace_internal::info(s_out) << std::vformat(fmt, args);
    #endif
}
void logWarning(const std::string& fmt, std::format_args args) {
    #if defined(LOG_LEVEL) && LOG_LEVEL <= 2
    std::scoped_lock<std::mutex> lock {__pcm_trace_internal::GStandardLock};
    __pcm_trace_internal::warning(s_out) << std::vformat(fmt, args);
    #endif
}