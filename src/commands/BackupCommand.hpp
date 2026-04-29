#pragma once

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

namespace cfgsync::commands {

class BackupCommand {
public:
    BackupCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void Execute() const;

private:
    core::Registry& Registry_;                 // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
