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

    // Redirects log output to a vector during the test and restores everything on exit
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

TEST_CASE("Messages reach the sink with level, category, and formatted text") {
    LogCapture capture;
    Log::setMinLevel(LogLevel::Trace);
    MGE_LOG_WARN("Audio", "Could not load ", "jump.wav", " (", 3, " attempts)");
    REQUIRE(capture.records.size() == 1);
    CHECK(capture.records[0].level == LogLevel::Warn);
    CHECK(capture.records[0].category == "Audio");
    CHECK(capture.records[0].message == "Could not load jump.wav (3 attempts)");
}

TEST_CASE("Minimum level filters messages and does not evaluate their arguments") {
    LogCapture capture;
    Log::setMinLevel(LogLevel::Error);
    evaluationCount = 0;
    MGE_LOG_WARN("Test", "value ", countEvaluation());
    MGE_LOG_ERROR("Test", "value ", countEvaluation());
    REQUIRE(capture.records.size() == 1);
    CHECK(capture.records[0].level == LogLevel::Error);
    CHECK(evaluationCount == 1); // Only the message that passed the filter
}

TEST_CASE("LogLevel::Off silences everything") {
    LogCapture capture;
    Log::setMinLevel(LogLevel::Off);
    MGE_LOG_FATAL("Test", "should not be emitted");
    CHECK(capture.records.empty());
    CHECK_FALSE(Log::isEnabled(LogLevel::Off));
}

TEST_CASE("Level names") {
    CHECK(std::string(Log::levelName(LogLevel::Info)) == "INFO");
    CHECK(std::string(Log::levelName(LogLevel::Fatal)) == "FATAL");
}

TEST_CASE("MGE_VERIFY returns the condition and logs an Error on failure") {
    LogCapture capture;
    Log::setMinLevel(LogLevel::Trace);
    int* missing = nullptr;
    CHECK(MGE_VERIFY(1 + 1 == 2, "never fails"));
    CHECK(capture.records.empty());
    CHECK_FALSE(MGE_VERIFY(missing != nullptr, "missing pointer"));
    REQUIRE(capture.records.size() == 1);
    CHECK(capture.records[0].level == LogLevel::Error);
    CHECK(capture.records[0].message.find("missing != nullptr") != std::string::npos);
    CHECK(capture.records[0].message.find("missing pointer") != std::string::npos);
}

#ifndef NDEBUG
TEST_CASE("MGE_ASSERT calls the handler with the failed expression") {
    LogCapture capture;
    std::string failedExpression;
    Log::setAssertHandler(
        [&](const char* expression, const char*, int, const std::string&) { failedExpression = expression; });
    MGE_ASSERT(2 > 1, "does not fail");
    CHECK(failedExpression.empty());
    MGE_ASSERT(1 > 2, "fails");
    CHECK(failedExpression == "1 > 2");
    REQUIRE_FALSE(capture.records.empty());
    CHECK(capture.records.back().level == LogLevel::Fatal);
}
#endif
