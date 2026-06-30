#include "commands/RestoreCommand.hpp"

#include "Exceptions.hpp"
#include "commands/RestoreDryRunPreview.hpp"
#include "utils/FileUtils.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <cstddef>
#include <filesystem>
#include <format>
#include <string>

namespace cfgsync::commands {
namespace fs = std::filesystem;

namespace {

bool HasPathPrefix(const fs::path& path, const fs::path& prefix) {
    auto pathIterator = path.begin();
    auto prefixIterator = prefix.begin();
    for (; prefixIterator != prefix.end(); ++prefixIterator, ++pathIterator) {
        if (pathIterator == path.end() || *pathIterator != *prefixIterator) {
            return false;
        }
    }

    return true;
}

fs::path RemapDestinationPath(const fs::path& originalPath, const RestorePrefixRemap& remap) {
    if (!HasPathPrefix(originalPath, remap.FromPrefix)) {
        throw CommandError{"Tracked file is outside --from-prefix: " + originalPath.string()};
    }

    auto pathIterator = originalPath.begin();
    for (auto prefixIterator = remap.FromPrefix.begin(); prefixIterator != remap.FromPrefix.end(); ++prefixIterator) {
        ++pathIterator;
    }

    auto destinationPath = remap.ToPrefix;
    for (; pathIterator != originalPath.end(); ++pathIterator) {
        destinationPath /= *pathIterator;
    }

    return destinationPath.lexically_normal();
}

fs::path GetRestoreDestinationPath(const core::TrackedEntry& trackedEntry,
                                   const std::optional<RestorePrefixRemap>& remap) {
    const fs::path originalPath{trackedEntry.OriginalPath};
    if (!remap.has_value()) {
        return originalPath;
    }

    return RemapDestinationPath(originalPath, *remap);
}

}  // namespace

RestoreCommand::RestoreCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void RestoreCommand::ExecuteAll(const std::optional<RestorePrefixRemap>& remap, RestoreMode mode) const {
    const auto& trackedEntries = Registry_.GetTrackedEntries();
    if (trackedEntries.empty()) {
        utils::LogInfo("No files tracked.");
        return;
    }

    std::size_t failureCount = 0;
    for (const auto& trackedEntry : trackedEntries) {
        try {
            const auto destinationPath = GetRestoreDestinationPath(trackedEntry, remap);
            if (mode == RestoreMode::DryRun) {
                const auto storedPath = StorageManager_.ResolveStoredPath(trackedEntry);
                utils::RequireOrdinaryFile(storedPath);
                detail::PrintDryRunImpact(storedPath, destinationPath);
                continue;
            }

            StorageManager_.RestoreEntry(trackedEntry, destinationPath);
            utils::LogInfo("Restored file: " + trackedEntry.OriginalPath);
        } catch (const FileError& error) {
            ++failureCount;
            utils::LogWarn("Failed to restore file: " + trackedEntry.OriginalPath + ": " + error.what());
        } catch (const CommandError& error) {
            ++failureCount;
            utils::LogWarn("Failed to restore file: " + trackedEntry.OriginalPath + ": " + error.what());
        }
    }

    if (failureCount > 0) {
        throw CommandError{
            std::format("Restore completed with {} failure{}.", failureCount, failureCount == 1 ? "" : "s")};
    }
}

void RestoreCommand::ExecuteSingle(const std::filesystem::path& filePath,
                                   const std::optional<RestorePrefixRemap>& remap, RestoreMode mode) const {
    const auto normalizedPath = utils::NormalizePath(filePath);
    const auto* trackedEntry = Registry_.FindEntryByOriginalPath(normalizedPath);
    if (trackedEntry == nullptr) {
        throw CommandError{"File is not tracked: " + normalizedPath.string()};
    }

    const auto destinationPath = GetRestoreDestinationPath(*trackedEntry, remap);
    if (mode == RestoreMode::DryRun) {
        const auto storedPath = StorageManager_.ResolveStoredPath(*trackedEntry);
        utils::RequireOrdinaryFile(storedPath);
        detail::PrintDryRunImpact(storedPath, destinationPath);
        return;
    }

    StorageManager_.RestoreEntry(*trackedEntry, destinationPath);
    utils::LogInfo("Restored file: " + trackedEntry->OriginalPath);
}

}  // namespace cfgsync::commands
