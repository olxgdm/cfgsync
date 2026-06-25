#pragma once

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

namespace cfgsync::commands {

enum class BackupMode {
    RefreshChanged,
    MissingOnly,
    Force,
};

class BackupCommand {
public:
    BackupCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void Execute(BackupMode mode = BackupMode::RefreshChanged) const;

private:
    core::Registry& Registry_;                 // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
