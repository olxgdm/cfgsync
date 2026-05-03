#include "commands/BackupCommand.hpp"

#include "utils/LogUtils.hpp"

#include <cstddef>
#include <exception>
#include <format>
#include <stdexcept>
#include <string>

namespace cfgsync::commands {

BackupCommand::BackupCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void BackupCommand::Execute() const {
    const auto& trackedEntries = Registry_.GetTrackedEntries();
    if (trackedEntries.empty()) {
        utils::LogInfo("No files tracked.");
        return;
    }

    std::size_t failureCount = 0;
    for (const auto& trackedEntry : trackedEntries) {
        try {
            StorageManager_.BackupEntry(trackedEntry);
            utils::LogInfo("Backed up file: " + trackedEntry.OriginalPath);
        } catch (const std::exception& error) {
            ++failureCount;
            utils::LogWarn("Failed to back up file: " + trackedEntry.OriginalPath + ": " + error.what());
        }
    }

    if (failureCount > 0) {
        throw std::runtime_error{std::format("Backup completed with {} failure{}.", failureCount,
                                             failureCount == 1 ? "" : "s")};
    }
}

}  // namespace cfgsync::commands
