#include "storage/StorageManager.hpp"

#include "utils/FileUtils.hpp"

#include <utility>

namespace cfgsync::storage {

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

    return StorageRoot_ / entry.StoredRelativePath;
}

void StorageManager::BackupEntry(const core::TrackedEntry& entry) const {
    const std::filesystem::path sourcePath{entry.OriginalPath};
    const auto destinationPath = ResolveStoredPath(entry);

    utils::RequireOrdinaryFile(sourcePath);
    utils::CopyFile(sourcePath, destinationPath);
}

}  // namespace cfgsync::storage
