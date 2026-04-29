#include "core/Registry.hpp"

#include "utils/FileUtils.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <algorithm>
#include <fmt/format.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_set>
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

void ValidateStoredRelativePath(const std::string& storedRelativePath, const fs::path& registryPath,
                                const std::string& fieldName) {
    if (storedRelativePath.empty()) {
        throw MalformedRegistryError(registryPath, fieldName + " must not be empty.");
    }

    if (fs::path{storedRelativePath}.is_absolute()) {
        throw MalformedRegistryError(registryPath, fieldName + " must be relative.");
    }
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

        ValidateStoredRelativePath(storedRelativePath, registryPath,
                                   fmt::format(fmt::runtime("tracked_files[{}].stored_relative_path"), index));

        trackedEntries.push_back(TrackedEntry{
            .OriginalPath = utils::NormalizePath(fs::path{originalPath}).string(),
            .StoredRelativePath = std::move(storedRelativePath),
        });
    }

    std::unordered_set<std::string> originalPaths;
    for (const auto& trackedEntry : trackedEntries) {
        if (!originalPaths.insert(trackedEntry.OriginalPath).second) {
            throw MalformedRegistryError(registryPath, fmt::format(fmt::runtime("duplicate original_path entry '{}'."),
                                                                   trackedEntry.OriginalPath));
        }
    }

    return trackedEntries;
}

nlohmann::json BuildRegistryDocument(const fs::path& storageRoot, const std::vector<TrackedEntry>& trackedEntries) {
    nlohmann::json trackedFiles = nlohmann::json::array();
    for (const auto& trackedEntry : trackedEntries) {
        trackedFiles.push_back({
            {"original_path", trackedEntry.OriginalPath},
            {"stored_relative_path", trackedEntry.StoredRelativePath},
        });
    }

    return {
        {"version", CurrentRegistryVersion},
        {"storage_root", storageRoot.string()},
        {"tracked_files", trackedFiles},
    };
}

}  // namespace

Registry::Registry(fs::path registryPath) : RegistryPath_(std::move(registryPath)) {}

void Registry::SetRegistryPath(fs::path registryPath) { RegistryPath_ = std::move(registryPath); }

const fs::path& Registry::GetRegistryPath() const { return RegistryPath_; }

void Registry::SetStorageRoot(const fs::path& storageRoot) {
    StorageRoot_ = utils::NormalizePath(storageRoot);
    if (RegistryPath_.empty() && !StorageRoot_.empty()) {
        RegistryPath_ = StorageRoot_ / "registry.json";
    }
}

const fs::path& Registry::GetStorageRoot() const { return StorageRoot_; }

void Registry::Initialize(const fs::path& storageRoot) {
    SetStorageRoot(storageRoot);
    if (StorageRoot_.empty()) {
        throw std::invalid_argument{"Storage root must not be empty."};
    }

    if (RegistryPath_.empty()) {
        RegistryPath_ = StorageRoot_ / "registry.json";
    }

    if (fs::exists(RegistryPath_)) {
        Load();
        if (StorageRoot_ != utils::NormalizePath(storageRoot)) {
            throw std::runtime_error{
                fmt::format(fmt::runtime("cfgsync registry '{}' belongs to storage root '{}', not '{}'."),
                            RegistryPath_.string(), StorageRoot_.string(), utils::NormalizePath(storageRoot).string())};
        }
        utils::EnsureDirectoryExists(StorageRoot_ / "files");
        utils::LogInfo(std::string{"Using existing cfgsync registry at "} + RegistryPath_.string());
        return;
    }

    utils::EnsureDirectoryExists(StorageRoot_);
    utils::EnsureDirectoryExists(StorageRoot_ / "files");
    TrackedEntries_.clear();
    Save();
    utils::LogInfo(std::string{"Created cfgsync registry at "} + RegistryPath_.string());
}

const std::vector<TrackedEntry>& Registry::GetTrackedEntries() const { return TrackedEntries_; }

void Registry::Load() {
    if (RegistryPath_.empty()) {
        throw std::runtime_error{"Registry path must be set before loading."};
    }

    const auto document = ReadRegistryDocument(RegistryPath_);
    if (!document.is_object()) {
        throw MalformedRegistryError(RegistryPath_, "root value must be an object.");
    }

    ValidateRegistryVersion(document, RegistryPath_);

    StorageRoot_ = ReadStorageRoot(document, RegistryPath_);
    TrackedEntries_ = ReadTrackedEntries(document, RegistryPath_);
}

void Registry::Save() const {
    if (RegistryPath_.empty()) {
        throw std::runtime_error{"Registry path must be set before saving."};
    }

    if (StorageRoot_.empty()) {
        throw std::runtime_error{"Storage root must be set before saving the registry."};
    }

    if (RegistryPath_.has_parent_path()) {
        utils::EnsureDirectoryExists(RegistryPath_.parent_path());
    }

    std::ofstream output{RegistryPath_};
    if (!output) {
        throw std::runtime_error{
            fmt::format(fmt::runtime("Unable to write cfgsync registry: {}"), RegistryPath_.string())};
    }

    output << BuildRegistryDocument(StorageRoot_, TrackedEntries_).dump(4) << '\n';
}

bool Registry::AddEntry(TrackedEntry entry) {
    const auto normalizedOriginalPath = utils::NormalizePath(fs::path{entry.OriginalPath});
    if (normalizedOriginalPath.empty()) {
        throw std::invalid_argument{"Tracked entry original path must not be empty."};
    }

    ValidateStoredRelativePath(entry.StoredRelativePath, RegistryPath_, "stored_relative_path");

    if (ContainsOriginalPath(normalizedOriginalPath)) {
        return false;
    }

    entry.OriginalPath = normalizedOriginalPath.string();
    TrackedEntries_.push_back(std::move(entry));
    return true;
}

bool Registry::RemoveEntry(const fs::path& originalPath) {
    const auto normalizedOriginalPath = utils::NormalizePath(originalPath).string();
    const auto entry = std::find_if(TrackedEntries_.begin(), TrackedEntries_.end(), [&](const TrackedEntry& candidate) {
        return candidate.OriginalPath == normalizedOriginalPath;
    });

    if (entry == TrackedEntries_.end()) {
        return false;
    }

    TrackedEntries_.erase(entry);
    return true;
}

const TrackedEntry* Registry::FindEntryByOriginalPath(const fs::path& originalPath) const {
    const auto normalizedOriginalPath = utils::NormalizePath(originalPath).string();
    const auto entry = std::find_if(TrackedEntries_.begin(), TrackedEntries_.end(), [&](const TrackedEntry& candidate) {
        return candidate.OriginalPath == normalizedOriginalPath;
    });

    if (entry == TrackedEntries_.end()) {
        return nullptr;
    }

    return &(*entry);
}

bool Registry::ContainsOriginalPath(const fs::path& originalPath) const {
    return FindEntryByOriginalPath(originalPath) != nullptr;
}

}  // namespace cfgsync::core
