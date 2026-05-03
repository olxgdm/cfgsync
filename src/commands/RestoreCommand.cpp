#include "commands/RestoreCommand.hpp"

#include "Exceptions.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <cstddef>
#include <format>
#include <string>

namespace cfgsync::commands {

RestoreCommand::RestoreCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void RestoreCommand::ExecuteAll() const {
    const auto& trackedEntries = Registry_.GetTrackedEntries();
    if (trackedEntries.empty()) {
        utils::LogInfo("No files tracked.");
        return;
    }

    std::size_t failureCount = 0;
    for (const auto& trackedEntry : trackedEntries) {
        try {
            StorageManager_.RestoreEntry(trackedEntry);
            utils::LogInfo("Restored file: " + trackedEntry.OriginalPath);
        } catch (const FileError& error) {
            ++failureCount;
            utils::LogWarn("Failed to restore file: " + trackedEntry.OriginalPath + ": " + error.what());
        }
    }

    if (failureCount > 0) {
        throw CommandError{std::format("Restore completed with {} failure{}.", failureCount,
                                       failureCount == 1 ? "" : "s")};
    }
}

void RestoreCommand::ExecuteSingle(const std::filesystem::path& filePath) const {
    const auto normalizedPath = utils::NormalizePath(filePath);
    const auto* trackedEntry = Registry_.FindEntryByOriginalPath(normalizedPath);
    if (trackedEntry == nullptr) {
        throw CommandError{"File is not tracked: " + normalizedPath.string()};
    }

    StorageManager_.RestoreEntry(*trackedEntry);
    utils::LogInfo("Restored file: " + trackedEntry->OriginalPath);
}

}  // namespace cfgsync::commands
