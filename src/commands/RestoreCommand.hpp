#pragma once

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

#include <filesystem>
#include <optional>

namespace cfgsync::commands {

struct RestorePrefixRemap {
    std::filesystem::path FromPrefix;
    std::filesystem::path ToPrefix;
};

class RestoreCommand {
public:
    RestoreCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void ExecuteAll(const std::optional<RestorePrefixRemap>& remap = std::nullopt) const;
    void ExecuteSingle(const std::filesystem::path& filePath,
                       const std::optional<RestorePrefixRemap>& remap = std::nullopt) const;

private:
    core::Registry& Registry_;                 // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
