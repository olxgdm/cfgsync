#pragma once

#include "core/AppConfig.hpp"
#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

#include <filesystem>

namespace cfgsync::commands {

class InitCommand {
public:
    InitCommand(core::Registry& registry, storage::StorageManager& storageManager, core::AppConfig& appConfig);

    void Execute(const std::filesystem::path& storageRoot);

private:
    core::Registry& Registry_;
    storage::StorageManager& StorageManager_;
    core::AppConfig& AppConfig_;
};

}  // namespace cfgsync::commands
