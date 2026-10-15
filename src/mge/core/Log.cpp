#include <mge/core/Log.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace mge {

    namespace {
        // Estado global del logger. Se accede siempre bajo el mutex.
        struct LogState {
            std::mutex mutex;
#ifdef NDEBUG
            LogLevel minLevel = LogLevel::Info;
#else
            LogLevel minLevel = LogLevel::Trace;
#endif
            LogSink sink;
            AssertHandler assertHandler;
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        };

        LogState& state() {
            static LogState s; // Construido en el primer uso (sin problemas de orden de inicialización)
            return s;
        }

        void writeToStderr(const LogRecord& record) {
            std::fprintf(stderr, "[%9.3f] [%-5s] [%.*s] %.*s\n", record.seconds, Log::levelName(record.level),
                         static_cast<int>(record.category.size()), record.category.data(),
                         static_cast<int>(record.message.size()), record.message.data());
            std::fflush(stderr);
        }
    }

    void Log::setMinLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(state().mutex);
        state().minLevel = level;
    }

    LogLevel Log::getMinLevel() {
        std::lock_guard<std::mutex> lock(state().mutex);
        return state().minLevel;
    }

    bool Log::isEnabled(LogLevel level) {
        return level != LogLevel::Off && level >= getMinLevel();
    }

    void Log::setSink(LogSink sink) {
        std::lock_guard<std::mutex> lock(state().mutex);
        state().sink = std::move(sink);
    }

    void Log::write(LogLevel level, std::string_view category, std::string_view message) {
        std::lock_guard<std::mutex> lock(state().mutex);
        std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - state().start;
        LogRecord record{level, category, message, elapsed.count()};
        if (state().sink)
            state().sink(record);
        else
            writeToStderr(record);
    }

    const char* Log::levelName(LogLevel level) {
        switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        case LogLevel::Off: return "OFF";
        }
        return "?";
    }

    void Log::setAssertHandler(AssertHandler handler) {
        std::lock_guard<std::mutex> lock(state().mutex);
        state().assertHandler = std::move(handler);
    }

    void Log::assertFailed(const char* expression, const char* file, int line, const std::string& message) {
        write(LogLevel::Fatal, "Assert",
              format("Falló la aserción '", expression, "' en ", file, ":", line,
                     message.empty() ? "" : " - ", message));

        AssertHandler handler;
        {
            std::lock_guard<std::mutex> lock(state().mutex);
            handler = state().assertHandler;
        }
        if (handler)
            handler(expression, file, line, message);
        else
            std::abort();
    }

    bool Log::verify(bool condition, const char* expression, const char* file, int line,
                     const std::string& message) {
        if (!condition) {
            write(LogLevel::Error, "Verify",
                  format("Falló '", expression, "' en ", file, ":", line, message.empty() ? "" : " - ",
                         message));
        }
        return condition;
    }
}
