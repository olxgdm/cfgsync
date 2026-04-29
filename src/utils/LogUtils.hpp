#pragma once

#include <spdlog/spdlog.h>
#include <string>

namespace cfgsync::utils {

inline void LogDebug(const std::string& message) {
    spdlog::default_logger_raw()->log(spdlog::level::debug, spdlog::string_view_t{message.data(), message.size()});
}

inline void LogInfo(const std::string& message) {
    spdlog::default_logger_raw()->log(spdlog::level::info, spdlog::string_view_t{message.data(), message.size()});
}

}  // namespace cfgsync::utils
