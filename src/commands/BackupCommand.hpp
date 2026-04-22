#pragma once

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

namespace cfgsync::commands {

class BackupCommand {
public:
    BackupCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void Execute() const;

private:
    core::Registry& Registry_;
    storage::StorageManager& StorageManager_;
};

}  // namespace cfgsync::commands
