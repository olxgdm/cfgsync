#include "watch/WatchBackupProcessor.hpp"

#include "Exceptions.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>
#include <vector>

namespace cfgsync::watch {
namespace fs = std::filesystem;

namespace {

std::string NormalizeLookupKey(const fs::path& path) { return utils::NormalizePath(path).string(); }

std::optional<fs::path> BackupCandidatePath(const FileWatchEvent& event) {
    using enum FileWatchAction;

    switch (event.Action) {
        case Added:
        case Modified:
        case Moved:
            return event.Path;
        case Deleted:
            return std::nullopt;
    }

    return std::nullopt;
}

}  // namespace

WatchBackupProcessor::WatchBackupProcessor(std::vector<core::TrackedEntry> trackedEntries,
                                           storage::StorageManager& storageManager,
                                           std::chrono::milliseconds debounceDelay)
    : StorageManager_(storageManager), DebounceDelay_(debounceDelay) {
    for (auto& entry : trackedEntries) {
        TrackedEntriesByPath_.emplace(NormalizeLookupKey(entry.OriginalPath), std::move(entry));
    }
}

void WatchBackupProcessor::OnFileChanged(const FileWatchEvent& event) { OnFileChangedAt(event, Clock::now()); }

void WatchBackupProcessor::OnFileChangedAt(const FileWatchEvent& event, TimePoint now) {
    using enum FileWatchAction;

    if (event.Action == Deleted) {
        const auto entry = FindTrackedEntry(event.Path);
        if (entry.has_value()) {
            utils::LogWarn("Tracked file was deleted: " + entry->OriginalPath);
        }
        return;
    }

    const auto candidatePath = BackupCandidatePath(event);
    if (!candidatePath.has_value()) {
        return;
    }

    const auto normalizedPath = NormalizeLookupKey(candidatePath.value());
    const auto entryIt = TrackedEntriesByPath_.find(normalizedPath);
    if (entryIt == TrackedEntriesByPath_.end()) {
        return;
    }

    std::scoped_lock lock{Mutex_};
    PendingBackups_[normalizedPath] = PendingBackup{
        .Entry = entryIt->second,
        .DueAt = now + DebounceDelay_,
    };
}

std::size_t WatchBackupProcessor::ProcessDueBackups() { return ProcessDueBackupsAt(Clock::now()); }

std::size_t WatchBackupProcessor::ProcessDueBackupsAt(TimePoint now) {
    std::vector<PendingBackup> dueBackups;
    {
        std::scoped_lock lock{Mutex_};
        for (auto it = PendingBackups_.begin(); it != PendingBackups_.end();) {
            if (it->second.DueAt > now) {
                ++it;
                continue;
            }

            dueBackups.push_back(std::move(it->second));
            it = PendingBackups_.erase(it);
        }
    }

    for (const auto& pendingBackup : dueBackups) {
        try {
            StorageManager_.BackupEntry(pendingBackup.Entry);
            utils::LogInfo("Backed up file: " + pendingBackup.Entry.OriginalPath);
        } catch (const FileError& error) {
            utils::LogWarn("Failed to back up file: " + pendingBackup.Entry.OriginalPath + ": " + error.what());
        }
    }

    return dueBackups.size();
}

std::optional<WatchBackupProcessor::TimePoint> WatchBackupProcessor::GetNextDueTime() const {
    std::scoped_lock lock{Mutex_};
    if (PendingBackups_.empty()) {
        return std::nullopt;
    }

    return std::min_element(PendingBackups_.begin(), PendingBackups_.end(),
                            [](const auto& left, const auto& right) {
                                return left.second.DueAt < right.second.DueAt;
                            })
        ->second.DueAt;
}

bool WatchBackupProcessor::HasPendingBackups() const {
    std::scoped_lock lock{Mutex_};
    return !PendingBackups_.empty();
}

std::optional<core::TrackedEntry> WatchBackupProcessor::FindTrackedEntry(const fs::path& path) const {
    const auto entryIt = TrackedEntriesByPath_.find(NormalizeLookupKey(path));
    if (entryIt == TrackedEntriesByPath_.end()) {
        return std::nullopt;
    }

    return entryIt->second;
}

}  // namespace cfgsync::watch
