#include "commands/BackupCommand.hpp"

#include "utils/LogUtils.hpp"

#include <cstddef>
#include <exception>
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
        const auto suffix = failureCount == 1 ? std::string{} : std::string{"s"};
        throw std::runtime_error{"Backup completed with " + std::to_string(failureCount) + " failure" + suffix + "."};
    }
}

}  // namespace cfgsync::commands
