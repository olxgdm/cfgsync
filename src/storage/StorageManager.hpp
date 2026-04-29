#pragma once

#include "core/TrackedEntry.hpp"

#include <filesystem>

namespace cfgsync::storage {

class StorageManager {
public:
    explicit StorageManager(std::filesystem::path storageRoot = {});

    void SetStorageRoot(std::filesystem::path storageRoot);
    const std::filesystem::path& GetStorageRoot() const;

    std::filesystem::path GetRegistryPath() const;
    std::filesystem::path ResolveStoredPath(const core::TrackedEntry& entry) const;

private:
    std::filesystem::path StorageRoot_;
};

}  // namespace cfgsync::storage
