#pragma once

#include <filesystem>

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

namespace cfgsync::commands {

class RestoreCommand {
public:
    RestoreCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void ExecuteAll() const;
    void ExecuteSingle(const std::filesystem::path& filePath) const;

private:
    core::Registry& Registry_;
    storage::StorageManager& StorageManager_;
};

}  // namespace cfgsync::commands
