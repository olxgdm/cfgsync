#pragma once

#include <filesystem>

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

namespace cfgsync::commands {

class InitCommand {
public:
    InitCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void Execute(const std::filesystem::path& storageRoot);

private:
    core::Registry& Registry_;
    storage::StorageManager& StorageManager_;
};

}  // namespace cfgsync::commands
