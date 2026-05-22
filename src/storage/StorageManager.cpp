#include "storage/StorageManager.hpp"

#include "Exceptions.hpp"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <string>
#include <utility>

namespace cfgsync::storage {
namespace fs = std::filesystem;

namespace {

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
    const auto validationError = utils::ValidateStoredRelativePath(storedRelativePath);
    if (validationError != utils::StoredRelativePathValidationError::None) {
        throw FileError{"stored_relative_path " +
                        std::string{utils::DescribeStoredRelativePathValidationError(validationError)}};
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
