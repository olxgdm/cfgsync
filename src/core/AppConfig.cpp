#include "core/AppConfig.hpp"

#include "Exceptions.hpp"
#include "utils/FileUtils.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <fmt/format.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace cfgsync::core {
namespace fs = std::filesystem;

namespace {

constexpr int CurrentConfigVersion = 1;

ConfigError MissingConfigError(const fs::path& configPath) {
    return ConfigError{fmt::format(
        fmt::runtime("cfgsync has not been initialized. Run: cfgsync init --storage <dir>. Missing app config: {}"),
        configPath.string())};
}

}  // namespace

AppConfig::AppConfig(fs::path configPath) : ConfigPath_(std::move(configPath)) {}

void AppConfig::SetConfigPath(fs::path configPath) { ConfigPath_ = std::move(configPath); }

const fs::path& AppConfig::GetConfigPath() const { return ConfigPath_; }

void AppConfig::SetStorageRoot(const fs::path& storageRoot) { StorageRoot_ = utils::NormalizePath(storageRoot); }

const fs::path& AppConfig::GetStorageRoot() const { return StorageRoot_; }

void AppConfig::Load() {
    if (ConfigPath_.empty()) {
        throw ConfigError{"Unable to load cfgsync app config: config path is not set."};
    }

    if (!fs::exists(ConfigPath_)) {
        throw MissingConfigError(ConfigPath_);
    }

    std::ifstream input{ConfigPath_};
    if (!input) {
        throw ConfigError{fmt::format(fmt::runtime("Unable to open cfgsync app config: {}"), ConfigPath_.string())};
    }

    nlohmann::json document;
    try {
        input >> document;
    } catch (const nlohmann::json::parse_error& error) {
        throw ConfigError{
            fmt::format(fmt::runtime("Malformed cfgsync app config '{}': {}"), ConfigPath_.string(), error.what())};
    }

    if (!document.is_object()) {
        throw ConfigError{fmt::format(fmt::runtime("Malformed cfgsync app config '{}': root value must be an object."),
                                      ConfigPath_.string())};
    }

    if (!document.contains("version") || !document["version"].is_number_integer()) {
        throw ConfigError{fmt::format(fmt::runtime("Malformed cfgsync app config '{}': version must be an integer."),
                                      ConfigPath_.string())};
    }

    const auto version = document["version"].get<int>();
    if (version != CurrentConfigVersion) {
        throw ConfigError{fmt::format(fmt::runtime("Unsupported cfgsync app config version {} in '{}'."), version,
                                      ConfigPath_.string())};
    }

    if (!document.contains("storage_root") || !document["storage_root"].is_string()) {
        throw ConfigError{fmt::format(fmt::runtime("Malformed cfgsync app config '{}': storage_root must be a string."),
                                      ConfigPath_.string())};
    }

    SetStorageRoot(fs::path{document["storage_root"].get<std::string>()});
    if (StorageRoot_.empty()) {
        throw ConfigError{fmt::format(
            fmt::runtime("Malformed cfgsync app config '{}': storage_root must not be empty."), ConfigPath_.string())};
    }

    utils::LogDebug(std::string{"Loaded cfgsync app config from "} + ConfigPath_.string());
}

void AppConfig::Save() const {
    if (ConfigPath_.empty()) {
        throw ConfigError{"Unable to save cfgsync app config: config path is not set."};
    }

    if (StorageRoot_.empty()) {
        throw ConfigError{"Unable to save cfgsync app config: storage root is not set."};
    }

    if (ConfigPath_.has_parent_path()) {
        utils::EnsureDirectoryExists(ConfigPath_.parent_path());
    }

    const nlohmann::json document = {
        {"version", CurrentConfigVersion},
        {"storage_root", StorageRoot_.string()},
    };

    std::ofstream output{ConfigPath_};
    if (!output) {
        throw ConfigError{fmt::format(fmt::runtime("Unable to write cfgsync app config: {}"), ConfigPath_.string())};
    }

    output << document.dump(4) << '\n';
    utils::LogInfo(std::string{"Saved cfgsync app config to "} + ConfigPath_.string());
}

}  // namespace cfgsync::core
