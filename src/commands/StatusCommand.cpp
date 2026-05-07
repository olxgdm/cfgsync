#include "commands/StatusCommand.hpp"

#include <iostream>
#include <ostream>

namespace cfgsync::commands {

namespace {

const char* StatusLabel(diff::FileStatus status) {
    using enum diff::FileStatus;

    switch (status) {
        case Modified:
            return "modified";
        case MissingOriginal:
            return "missing-original";
        case MissingBackup:
            return "missing-backup";
        case Clean:
            return "";
    }

    return "";
}

}  // namespace

StatusCommand::StatusCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void StatusCommand::Execute(std::ostream& output) const {
    const diff::FileComparator comparator{StorageManager_};
    bool hasChanges = false;

    for (const auto& result : comparator.CompareAll(Registry_.GetTrackedEntries())) {
        if (result.Status == diff::FileStatus::Clean) {
            continue;
        }

        hasChanges = true;
        output << StatusLabel(result.Status) << ' ' << result.Entry.OriginalPath << '\n';
    }

    if (!hasChanges) {
        output << "Clean.\n";
    }
}

void StatusCommand::Execute() const { Execute(std::cout); }

}  // namespace cfgsync::commands
