#include "core/Registry.hpp"

#include <utility>

namespace cfgsync::core {

Registry::Registry(std::filesystem::path registryPath)
    : RegistryPath_(std::move(registryPath)) {}

void Registry::SetRegistryPath(std::filesystem::path registryPath) {
    RegistryPath_ = std::move(registryPath);
}

const std::filesystem::path& Registry::GetRegistryPath() const {
    return RegistryPath_;
}

const std::vector<TrackedEntry>& Registry::GetTrackedEntries() const {
    return TrackedEntries_;
}

}  // namespace cfgsync::core
