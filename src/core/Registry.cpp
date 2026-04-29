#include "core/Registry.hpp"

#include "utils/FileUtils.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <fmt/format.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace cfgsync::core {
namespace fs = std::filesystem;

namespace {

constexpr int CurrentRegistryVersion = 1;

std::runtime_error MalformedRegistryError(const fs::path& registryPath, const std::string& message) {
    return std::runtime_error{
        fmt::format(fmt::runtime("Malformed cfgsync registry '{}': {}"), registryPath.string(), message)};
}

nlohmann::json ReadRegistryDocument(const fs::path& registryPath) {
    std::ifstream input{registryPath};
    if (!input) {
        throw std::runtime_error{
            fmt::format(fmt::runtime("Unable to open cfgsync registry: {}"), registryPath.string())};
    }

    nlohmann::json document;
    try {
        input >> document;
    } catch (const nlohmann::json::parse_error& error) {
        throw std::runtime_error{
            fmt::format(fmt::runtime("Malformed cfgsync registry '{}': {}"), registryPath.string(), error.what())};
    }

    return document;
}

void ValidateRegistryVersion(const nlohmann::json& document, const fs::path& registryPath) {
    if (!document.contains("version") || !document["version"].is_number_integer()) {
        throw MalformedRegistryError(registryPath, "version must be an integer.");
    }

    const auto version = document["version"].get<int>();
    if (version != CurrentRegistryVersion) {
        throw std::runtime_error{fmt::format(fmt::runtime("Unsupported cfgsync registry version {} in '{}'."), version,
                                             registryPath.string())};
    }
}

fs::path ReadStorageRoot(const nlohmann::json& document, const fs::path& registryPath) {
    if (!document.contains("storage_root") || !document["storage_root"].is_string()) {
        throw MalformedRegistryError(registryPath, "storage_root must be a string.");
    }

    const fs::path storageRoot{document["storage_root"].get<std::string>()};
    if (storageRoot.empty()) {
        throw MalformedRegistryError(registryPath, "storage_root must not be empty.");
    }

    return utils::NormalizePath(storageRoot);
}

std::vector<TrackedEntry> ReadTrackedEntries(const nlohmann::json& document, const fs::path& registryPath) {
    if (!document.contains("tracked_files") || !document["tracked_files"].is_array()) {
        throw MalformedRegistryError(registryPath, "tracked_files must be an array.");
    }

    std::vector<TrackedEntry> trackedEntries;
    for (std::size_t index = 0; index < document["tracked_files"].size(); ++index) {
        const auto& entry = document["tracked_files"][index];
        if (!entry.is_object()) {
            throw MalformedRegistryError(registryPath,
                                         fmt::format(fmt::runtime("tracked_files[{}] must be an object."), index));
        }

        if (!entry.contains("original_path") || !entry["original_path"].is_string()) {
            throw MalformedRegistryError(
                registryPath, fmt::format(fmt::runtime("tracked_files[{}].original_path must be a string."), index));
        }

        if (!entry.contains("stored_relative_path") || !entry["stored_relative_path"].is_string()) {
            throw MalformedRegistryError(
                registryPath,
                fmt::format(fmt::runtime("tracked_files[{}].stored_relative_path must be a string."), index));
        }

        auto originalPath = entry["original_path"].get<std::string>();
        auto storedRelativePath = entry["stored_relative_path"].get<std::string>();

        if (originalPath.empty()) {
            throw MalformedRegistryError(
                registryPath, fmt::format(fmt::runtime("tracked_files[{}].original_path must not be empty."), index));
        }

        if (storedRelativePath.empty()) {
            throw MalformedRegistryError(
                registryPath,
                fmt::format(fmt::runtime("tracked_files[{}].stored_relative_path must not be empty."), index));
        }

        trackedEntries.push_back(TrackedEntry{
            .OriginalPath = std::move(originalPath),
            .StoredRelativePath = std::move(storedRelativePath),
        });
    }

    return trackedEntries;
}

}  // namespace

Registry::Registry(fs::path registryPath) : RegistryPath_(std::move(registryPath)) {}

void Registry::SetRegistryPath(fs::path registryPath) { RegistryPath_ = std::move(registryPath); }

const fs::path& Registry::GetRegistryPath() const { return RegistryPath_; }

void Registry::Initialize(const fs::path& storageRoot) {
    const auto normalizedStorageRoot = utils::NormalizePath(storageRoot);
    if (normalizedStorageRoot.empty()) {
        throw std::invalid_argument{"Storage root must not be empty."};
    }

    if (RegistryPath_.empty()) {
        RegistryPath_ = normalizedStorageRoot / "registry.json";
    }

    utils::EnsureDirectoryExists(normalizedStorageRoot / "files");

    if (fs::exists(RegistryPath_)) {
        utils::LogInfo(std::string{"Using existing cfgsync registry at "} + RegistryPath_.string());
        return;
    }

    if (RegistryPath_.has_parent_path()) {
        utils::EnsureDirectoryExists(RegistryPath_.parent_path());
    }

    const nlohmann::json document = {
        {"version", CurrentRegistryVersion},
        {"storage_root", normalizedStorageRoot.string()},
        {"tracked_files", nlohmann::json::array()},
    };

    std::ofstream output{RegistryPath_};
    if (!output) {
        throw std::runtime_error{
            fmt::format(fmt::runtime("Unable to write cfgsync registry: {}"), RegistryPath_.string())};
    }

    output << document.dump(4) << '\n';
}

}  // namespace cfgsync::core
