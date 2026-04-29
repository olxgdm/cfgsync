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

    void Initialize(const std::filesystem::path& storageRoot);

    const std::vector<TrackedEntry>& GetTrackedEntries() const;

private:
    std::filesystem::path RegistryPath_;
    std::vector<TrackedEntry> TrackedEntries_;
};

}  // namespace cfgsync::core
