// Este archivo compila el log con nivel mínimo Warn: Trace/Debug/Info desaparecen del binario.
#define MGE_LOG_COMPILE_LEVEL 3
#include <doctest/doctest.h>
#include <mge/core/Log.h>

using namespace mge;

namespace {
    int strippedEvaluationCount = 0;
    int countStripped() {
        strippedEvaluationCount++;
        return 1;
    }
}

TEST_CASE("Los niveles compilados fuera no evalúan sus argumentos") {
    LogLevel previous = Log::getMinLevel();
    Log::setMinLevel(LogLevel::Trace);
    Log::setSink([](const LogRecord&) {});

    strippedEvaluationCount = 0;
    MGE_LOG_TRACE("Test", countStripped());
    MGE_LOG_DEBUG("Test", countStripped());
    MGE_LOG_INFO("Test", countStripped());
    CHECK(strippedEvaluationCount == 0);

    MGE_LOG_WARN("Test", countStripped());
    CHECK(strippedEvaluationCount == 1);

    Log::setSink(nullptr);
    Log::setMinLevel(previous);
}
