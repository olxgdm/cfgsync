#pragma once

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

#include <filesystem>

namespace cfgsync::commands {

class RestoreCommand {
public:
    RestoreCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void ExecuteAll() const;
    void ExecuteSingle(const std::filesystem::path& filePath) const;

private:
    core::Registry& Registry_;                 // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
