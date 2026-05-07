#include "commands/DiffCommand.hpp"

#include "Exceptions.hpp"
#include "diff/FileComparator.hpp"
#include "diff/UnifiedDiff.hpp"
#include "utils/PathUtils.hpp"
#include "utils/TerminalStyle.hpp"

#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>

namespace cfgsync::commands {
namespace fs = std::filesystem;

namespace {

std::string BuildDisplayPath(const core::TrackedEntry& entry) {
    fs::path displayPath;
    bool skippedStoragePrefix = false;

    for (const auto& component : fs::path{entry.StoredRelativePath}) {
        if (!skippedStoragePrefix && component == "files") {
            skippedStoragePrefix = true;
            continue;
        }

        displayPath /= component;
    }

    const auto displayPathText = displayPath.generic_string();
    return displayPathText.empty() ? fs::path{entry.StoredRelativePath}.generic_string() : displayPathText;
}

CommandError MissingBackupError(const core::TrackedEntry& entry, const fs::path& storedPath) {
    return CommandError{"Tracked file has no stored backup yet; run cfgsync backup before diffing: " +
                        entry.OriginalPath + " (expected backup: " + storedPath.string() + ")"};
}

}  // namespace

DiffCommand::DiffCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void DiffCommand::Execute(const fs::path& filePath, std::ostream& output) const {
    const auto normalizedPath = utils::NormalizePath(filePath);
    const auto* trackedEntry = Registry_.FindEntryByOriginalPath(normalizedPath);
    if (trackedEntry == nullptr) {
        throw CommandError{"File is not tracked: " + normalizedPath.string()};
    }

    const diff::FileComparator comparator{StorageManager_};
    const auto status = comparator.Compare(*trackedEntry);
    const auto storedPath = StorageManager_.ResolveStoredPath(*trackedEntry);

    switch (status.Status) {
        case diff::FileStatus::Clean:
            return;
        case diff::FileStatus::MissingOriginal:
            throw CommandError{"Original file is missing: " + trackedEntry->OriginalPath};
        case diff::FileStatus::MissingBackup:
            throw MissingBackupError(*trackedEntry, storedPath);
        case diff::FileStatus::Modified:
            break;
    }

    const auto displayPath = BuildDisplayPath(*trackedEntry);
    const auto plainDiff = diff::GenerateUnifiedDiff(diff::ReadTextFileForDiff(storedPath),
                                                     diff::ReadTextFileForDiff(fs::path{trackedEntry->OriginalPath}),
                                                     "stored/" + displayPath, "original/" + displayPath);
    output << diff::RenderUnifiedDiff(plainDiff, utils::Colorizer::Enabled());
}

void DiffCommand::Execute(const fs::path& filePath) const { Execute(filePath, std::cout); }

}  // namespace cfgsync::commands
