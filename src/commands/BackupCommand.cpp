#include "commands/BackupCommand.hpp"

#include "Exceptions.hpp"
#include "utils/LogUtils.hpp"

#include <cstddef>
#include <filesystem>
#include <format>
#include <string>

namespace cfgsync::commands {
namespace fs = std::filesystem;

namespace {

bool StoredBackupExists(const fs::path& storedPath) {
    std::error_code errorCode;
    const auto exists = fs::exists(storedPath, errorCode);
    if (errorCode) {
        throw FileError{std::format("Unable to inspect stored backup '{}': {}", storedPath.string(),
                                    errorCode.message())};
    }

    return exists;
}

}  // namespace

BackupCommand::BackupCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void BackupCommand::Execute() const {
    const auto& trackedEntries = Registry_.GetTrackedEntries();
    if (trackedEntries.empty()) {
        utils::LogInfo("No files tracked.");
        return;
    }

    std::size_t failureCount = 0;
    std::size_t backupCount = 0;
    for (const auto& trackedEntry : trackedEntries) {
        try {
            if (StoredBackupExists(StorageManager_.ResolveStoredPath(trackedEntry))) {
                continue;
            }

            StorageManager_.BackupEntry(trackedEntry);
            ++backupCount;
            utils::LogInfo("Backed up file: " + trackedEntry.OriginalPath);
        } catch (const FileError& error) {
            ++failureCount;
            utils::LogWarn("Failed to back up file: " + trackedEntry.OriginalPath + ": " + error.what());
        }
    }

    if (failureCount > 0) {
        throw CommandError{
            std::format("Backup completed with {} failure{}.", failureCount, failureCount == 1 ? "" : "s")};
    }

    if (backupCount == 0) {
        utils::LogInfo("No new files to back up.");
    }
}

}  // namespace cfgsync::commands
