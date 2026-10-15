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

    // Un mensaje ya formateado, tal como lo recibe un sink
    struct LogRecord {
        LogLevel level;
        std::string_view category;
        std::string_view message;
        double seconds; // Tiempo desde el arranque del proceso
    };

    using LogSink = std::function<void(const LogRecord&)>;
    using AssertHandler =
        std::function<void(const char* expression, const char* file, int line, const std::string& message)>;

    // Sistema de logging del motor (sin dependencias: vive en el core).
    // Usar las macros MGE_LOG_* en vez de llamar a write() directamente.
    class Log {
    public:
        // Nivel mínimo en tiempo de ejecución (por defecto: Trace en Debug, Info en Release)
        static void setMinLevel(LogLevel level);
        static LogLevel getMinLevel();
        static bool isEnabled(LogLevel level);

        // Reemplaza el destino de los mensajes (nullptr = stderr por defecto). Útil en tests.
        static void setSink(LogSink sink);

        static void write(LogLevel level, std::string_view category, std::string_view message);

        static const char* levelName(LogLevel level);

        // Concatena los argumentos con operator<< (C++17, sin std::format)
        template <typename... Args> static std::string format(Args&&... args) {
            std::ostringstream stream;
            (stream << ... << std::forward<Args>(args));
            return stream.str();
        }
        static std::string format() { return {}; }

        // Qué hacer cuando falla un MGE_ASSERT (por defecto: loguear Fatal y abortar)
        static void setAssertHandler(AssertHandler handler);
        static void assertFailed(const char* expression, const char* file, int line,
                                 const std::string& message);
        // Usado por MGE_VERIFY: loguea un Error si la condición es falsa y la devuelve
        static bool verify(bool condition, const char* expression, const char* file, int line,
                           const std::string& message);
    };
}

// --- Nivel de compilación: los niveles por debajo desaparecen del binario ---
// 0 = Trace ... 5 = Fatal. Por defecto: todo en Debug, desde Info en Release.
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

// Si un nivel está compilado fuera, sus argumentos no se evalúan
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
// Error y Fatal nunca se compilan fuera
#define MGE_LOG_ERROR(category, ...) MGE_LOG(::mge::LogLevel::Error, category, __VA_ARGS__)
#define MGE_LOG_FATAL(category, ...) MGE_LOG(::mge::LogLevel::Fatal, category, __VA_ARGS__)

// --- Aserciones ---
// MGE_ASSERT: solo en Debug (en Release no evalúa nada). Falla -> handler (por defecto: abort).
// MGE_VERIFY: se evalúa SIEMPRE y devuelve la condición; si es falsa loguea un Error.
//   if (!MGE_VERIFY(window != nullptr, "sin ventana")) return false;
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
