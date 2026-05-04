#pragma once

#include "core/AppConfig.hpp"
#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

#include <filesystem>

namespace cfgsync::commands {

class UseCommand {
public:
    UseCommand(core::Registry& registry, storage::StorageManager& storageManager, core::AppConfig& appConfig);

    void Execute(const std::filesystem::path& storageRoot);

private:
    core::Registry& Registry_;                 // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    core::AppConfig& AppConfig_;               // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
