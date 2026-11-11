// NOLINTBEGIN(readability-identifier-naming)
#include <mge/c/mge_c_internal.h>

#include <mge/Version.h>

#include <string>

namespace {
    thread_local std::string t_lastError;
}

namespace mge::c_impl {
    void setLastError(std::string message) {
        t_lastError = std::move(message);
    }
} // namespace mge::c_impl

extern "C" {

uint32_t mge_c_api_version(void) {
    return MGE_C_API_VERSION;
}

const char* mge_version_string(void) {
    return MGE_VERSION_STRING;
}

const char* mge_last_error(void) {
    return t_lastError.c_str();
}

void mge_log_info(const char* category, const char* message) {
    if (!category || !message) return;
    MGE_LOG_INFO(category, message);
}

void mge_log_warn(const char* category, const char* message) {
    if (!category || !message) return;
    MGE_LOG_WARN(category, message);
}

void mge_log_error(const char* category, const char* message) {
    if (!category || !message) return;
    MGE_LOG_ERROR(category, message);
}

} // extern "C"
// NOLINTEND(readability-identifier-naming)
