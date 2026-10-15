#include <doctest/doctest.h>
#include <mge/core/Log.h>

#include <string>
#include <vector>

using namespace mge;

namespace {
    struct Captured {
        LogLevel level;
        std::string category;
        std::string message;
    };

    // Redirige el log a un vector durante el test y restaura todo al salir
    struct LogCapture {
        std::vector<Captured> records;
        LogLevel previousLevel = Log::getMinLevel();

        LogCapture() {
            Log::setSink([this](const LogRecord& r) {
                records.push_back({r.level, std::string(r.category), std::string(r.message)});
            });
        }
        ~LogCapture() {
            Log::setSink(nullptr);
            Log::setMinLevel(previousLevel);
            Log::setAssertHandler(nullptr);
        }
    };

    int evaluationCount = 0;
    int countEvaluation() {
        evaluationCount++;
        return 42;
    }
}

TEST_CASE("Los mensajes llegan al sink con nivel, categoría y texto formateado") {
    LogCapture capture;
    Log::setMinLevel(LogLevel::Trace);
    MGE_LOG_WARN("Audio", "No se pudo cargar ", "jump.wav", " (", 3, " intentos)");
    REQUIRE(capture.records.size() == 1);
    CHECK(capture.records[0].level == LogLevel::Warn);
    CHECK(capture.records[0].category == "Audio");
    CHECK(capture.records[0].message == "No se pudo cargar jump.wav (3 intentos)");
}

TEST_CASE("El nivel mínimo filtra mensajes y no evalúa sus argumentos") {
    LogCapture capture;
    Log::setMinLevel(LogLevel::Error);
    evaluationCount = 0;
    MGE_LOG_WARN("Test", "valor ", countEvaluation());
    MGE_LOG_ERROR("Test", "valor ", countEvaluation());
    REQUIRE(capture.records.size() == 1);
    CHECK(capture.records[0].level == LogLevel::Error);
    CHECK(evaluationCount == 1); // Solo el mensaje que pasó el filtro
}

TEST_CASE("LogLevel::Off silencia todo") {
    LogCapture capture;
    Log::setMinLevel(LogLevel::Off);
    MGE_LOG_FATAL("Test", "no debería salir");
    CHECK(capture.records.empty());
    CHECK_FALSE(Log::isEnabled(LogLevel::Off));
}

TEST_CASE("Nombres de niveles") {
    CHECK(std::string(Log::levelName(LogLevel::Info)) == "INFO");
    CHECK(std::string(Log::levelName(LogLevel::Fatal)) == "FATAL");
}

TEST_CASE("MGE_VERIFY devuelve la condición y loguea un Error si falla") {
    LogCapture capture;
    Log::setMinLevel(LogLevel::Trace);
    int* missing = nullptr;
    CHECK(MGE_VERIFY(1 + 1 == 2, "nunca falla"));
    CHECK(capture.records.empty());
    CHECK_FALSE(MGE_VERIFY(missing != nullptr, "falta el puntero"));
    REQUIRE(capture.records.size() == 1);
    CHECK(capture.records[0].level == LogLevel::Error);
    CHECK(capture.records[0].message.find("missing != nullptr") != std::string::npos);
    CHECK(capture.records[0].message.find("falta el puntero") != std::string::npos);
}

#ifndef NDEBUG
TEST_CASE("MGE_ASSERT llama al handler con la expresión que falló") {
    LogCapture capture;
    std::string failedExpression;
    Log::setAssertHandler(
        [&](const char* expression, const char*, int, const std::string&) { failedExpression = expression; });
    MGE_ASSERT(2 > 1, "no falla");
    CHECK(failedExpression.empty());
    MGE_ASSERT(1 > 2, "falla");
    CHECK(failedExpression == "1 > 2");
    REQUIRE_FALSE(capture.records.empty());
    CHECK(capture.records.back().level == LogLevel::Fatal);
}
#endif
