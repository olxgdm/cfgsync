#include "storage/StorageManager.hpp"

#include "Exceptions.hpp"
#include "utils/FileUtils.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cfgsync::storage {
namespace fs = std::filesystem;

namespace {

bool IsStoredPathRooted(std::string_view storedRelativePath) {
    if (storedRelativePath.empty()) {
        return false;
    }

    if (storedRelativePath[0] == '/' || storedRelativePath[0] == '\\') {
        return true;
    }

    return storedRelativePath.size() >= 2 && std::isalpha(static_cast<unsigned char>(storedRelativePath[0])) != 0 &&
           storedRelativePath[1] == ':';
}

std::vector<std::string> SplitStoredPathComponents(std::string_view storedRelativePath) {
    std::vector<std::string> components;
    std::string component;
    for (const auto character : storedRelativePath) {
        if (character == '/' || character == '\\') {
            if (!component.empty()) {
                components.push_back(component);
                component.clear();
            }
            continue;
        }

        component.push_back(character);
    }

    if (!component.empty()) {
        components.push_back(component);
    }

    return components;
}

bool HasPathPrefix(const fs::path& path, const fs::path& prefix) {
    auto pathIterator = path.begin();
    auto prefixIterator = prefix.begin();
    for (; prefixIterator != prefix.end(); ++prefixIterator, ++pathIterator) {
        if (pathIterator == path.end() || *pathIterator != *prefixIterator) {
            return false;
        }
    }

    return true;
}

void ValidateStoredRelativePath(const std::string& storedRelativePath) {
    if (storedRelativePath.empty()) {
        throw FileError{"stored_relative_path must not be empty."};
    }

    if (fs::path{storedRelativePath}.is_absolute() || IsStoredPathRooted(storedRelativePath)) {
        throw FileError{"stored_relative_path must be relative."};
    }

    const auto components = SplitStoredPathComponents(storedRelativePath);
    if (components.empty() || components.front() != "files") {
        throw FileError{"stored_relative_path must be under files/."};
    }

    if (components.size() < 2) {
        throw FileError{"stored_relative_path must include a path under files/."};
    }

    if (std::any_of(components.begin(), components.end(),
                    [](const std::string& component) { return component == ".."; })) {
        throw FileError{"stored_relative_path must not contain parent directory traversal."};
    }
}

}  // namespace

StorageManager::StorageManager(std::filesystem::path storageRoot) : StorageRoot_(std::move(storageRoot)) {}

void StorageManager::SetStorageRoot(std::filesystem::path storageRoot) { StorageRoot_ = std::move(storageRoot); }

const std::filesystem::path& StorageManager::GetStorageRoot() const { return StorageRoot_; }

std::filesystem::path StorageManager::GetRegistryPath() const {
    if (StorageRoot_.empty()) {
        return {};
    }

    return StorageRoot_ / "registry.json";
}

std::filesystem::path StorageManager::ResolveStoredPath(const core::TrackedEntry& entry) const {
    if (StorageRoot_.empty()) {
        return {};
    }

    ValidateStoredRelativePath(entry.StoredRelativePath);

    const auto storageRoot = StorageRoot_.lexically_normal();
    const auto filesRoot = (storageRoot / "files").lexically_normal();
    const auto storedPath = (StorageRoot_ / entry.StoredRelativePath).lexically_normal();
    if (!HasPathPrefix(storedPath, filesRoot)) {
        throw FileError{"stored_relative_path resolves outside cfgsync storage files."};
    }

    return storedPath;
}

void StorageManager::BackupEntry(const core::TrackedEntry& entry) const {
    const std::filesystem::path sourcePath{entry.OriginalPath};
    const auto destinationPath = ResolveStoredPath(entry);

    utils::RequireOrdinaryFile(sourcePath);
    utils::CopyFile(sourcePath, destinationPath);
}

void StorageManager::RestoreEntry(const core::TrackedEntry& entry) const {
    const auto sourcePath = ResolveStoredPath(entry);
    const std::filesystem::path destinationPath{entry.OriginalPath};

    utils::RequireOrdinaryFile(sourcePath);
    utils::CopyFile(sourcePath, destinationPath);
}

}  // namespace cfgsync::storage
