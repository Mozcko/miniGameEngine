#pragma once
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace mge {

    enum class LogLevel {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal,
        Off
    };

    // An already formatted message, as received by a sink
    struct LogRecord {
        LogLevel level;
        std::string_view category;
        std::string_view message;
        double seconds; // Time since process startup
    };

    using LogSink = std::function<void(const LogRecord&)>;
    using AssertHandler =
        std::function<void(const char* expression, const char* file, int line, const std::string& message)>;

    // Engine logging system (dependency-free: lives in core).
    // Use the MGE_LOG_* macros instead of calling write() directly.
    class Log {
    public:
        // Minimum runtime level (default: Trace in Debug, Info in Release)
        static void setMinLevel(LogLevel level);
        static LogLevel getMinLevel();
        static bool isEnabled(LogLevel level);

        // Replaces the message destination (nullptr = default stderr). Useful in tests.
        static void setSink(LogSink sink);

        static void write(LogLevel level, std::string_view category, std::string_view message);

        static const char* levelName(LogLevel level);

        // Concatenates arguments using operator<< (C++17, no std::format)
        template <typename... Args> static std::string format(Args&&... args) {
            std::ostringstream stream;
            (stream << ... << std::forward<Args>(args));
            return stream.str();
        }
        static std::string format() { return {}; }

        // What to do when an MGE_ASSERT fails (default: log Fatal and abort)
        static void setAssertHandler(AssertHandler handler);
        static void assertFailed(const char* expression, const char* file, int line,
                                 const std::string& message);
        // Used by MGE_VERIFY: logs an Error if the condition is false and returns it
        static bool verify(bool condition, const char* expression, const char* file, int line,
                           const std::string& message);
    };
}

// --- Compile-time level: levels below this are stripped from the binary ---
// 0 = Trace ... 5 = Fatal. Default: all in Debug, Info and above in Release.
#ifndef MGE_LOG_COMPILE_LEVEL
#ifdef NDEBUG
#define MGE_LOG_COMPILE_LEVEL 2
#else
#define MGE_LOG_COMPILE_LEVEL 0
#endif
#endif

#define MGE_LOG(level, category, ...)                                                                        \
    do {                                                                                                     \
        if (::mge::Log::isEnabled(level))                                                                    \
            ::mge::Log::write(level, category, ::mge::Log::format(__VA_ARGS__));                             \
    } while (0)

// If a level is compiled out, its arguments are not evaluated
#define MGE_LOG_DISCARDED() ((void)0)

#if MGE_LOG_COMPILE_LEVEL <= 0
#define MGE_LOG_TRACE(category, ...) MGE_LOG(::mge::LogLevel::Trace, category, __VA_ARGS__)
#else
#define MGE_LOG_TRACE(category, ...) MGE_LOG_DISCARDED()
#endif
#if MGE_LOG_COMPILE_LEVEL <= 1
#define MGE_LOG_DEBUG(category, ...) MGE_LOG(::mge::LogLevel::Debug, category, __VA_ARGS__)
#else
#define MGE_LOG_DEBUG(category, ...) MGE_LOG_DISCARDED()
#endif
#if MGE_LOG_COMPILE_LEVEL <= 2
#define MGE_LOG_INFO(category, ...) MGE_LOG(::mge::LogLevel::Info, category, __VA_ARGS__)
#else
#define MGE_LOG_INFO(category, ...) MGE_LOG_DISCARDED()
#endif
#if MGE_LOG_COMPILE_LEVEL <= 3
#define MGE_LOG_WARN(category, ...) MGE_LOG(::mge::LogLevel::Warn, category, __VA_ARGS__)
#else
#define MGE_LOG_WARN(category, ...) MGE_LOG_DISCARDED()
#endif
// Error and Fatal are never compiled out
#define MGE_LOG_ERROR(category, ...) MGE_LOG(::mge::LogLevel::Error, category, __VA_ARGS__)
#define MGE_LOG_FATAL(category, ...) MGE_LOG(::mge::LogLevel::Fatal, category, __VA_ARGS__)

// --- Assertions ---
// MGE_ASSERT: Debug only (evaluates nothing in Release). Failure -> handler (default: abort).
// MGE_VERIFY: ALWAYS evaluated and returns the condition; logs an Error if false.
//   if (!MGE_VERIFY(window != nullptr, "no window")) return false;
#ifdef NDEBUG
#define MGE_ASSERT(condition, ...) ((void)0)
#else
#define MGE_ASSERT(condition, ...)                                                                           \
    do {                                                                                                     \
        if (!(condition))                                                                                    \
            ::mge::Log::assertFailed(#condition, __FILE__, __LINE__, ::mge::Log::format(__VA_ARGS__));       \
    } while (0)
#endif

#define MGE_VERIFY(condition, ...)                                                                           \
    ::mge::Log::verify(static_cast<bool>(condition), #condition, __FILE__, __LINE__,                         \
                       ::mge::Log::format(__VA_ARGS__))
