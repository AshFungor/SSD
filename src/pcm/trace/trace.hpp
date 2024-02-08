#pragma once

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
} // namespace pcm_log