#pragma once

#include "core/TrackedEntry.hpp"

#include <filesystem>
#include <vector>

namespace cfgsync::core {

class Registry {
public:
    explicit Registry(std::filesystem::path registryPath = {});

    void SetRegistryPath(std::filesystem::path registryPath);
    const std::filesystem::path& GetRegistryPath() const;

    void SetStorageRoot(std::filesystem::path storageRoot);
    const std::filesystem::path& GetStorageRoot() const;

    void Initialize(const std::filesystem::path& storageRoot);
    void Load();
    void Save() const;

    bool AddEntry(TrackedEntry entry);
    bool RemoveEntry(const std::filesystem::path& originalPath);
    const TrackedEntry* FindEntryByOriginalPath(const std::filesystem::path& originalPath) const;
    bool ContainsOriginalPath(const std::filesystem::path& originalPath) const;

    const std::vector<TrackedEntry>& GetTrackedEntries() const;

private:
    std::filesystem::path RegistryPath_;
    std::filesystem::path StorageRoot_;
    std::vector<TrackedEntry> TrackedEntries_;
};

}  // namespace cfgsync::core
