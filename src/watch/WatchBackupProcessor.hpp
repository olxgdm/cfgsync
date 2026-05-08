#pragma once

#include "core/TrackedEntry.hpp"
#include "storage/StorageManager.hpp"
#include "watch/FileWatchObserver.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cfgsync::watch {

class WatchBackupProcessor final : public FileWatchObserver {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    static constexpr std::chrono::milliseconds DefaultDebounceDelay{500};

    WatchBackupProcessor(std::vector<core::TrackedEntry> trackedEntries, storage::StorageManager& storageManager,
                         std::chrono::milliseconds debounceDelay = DefaultDebounceDelay);

    void OnFileChanged(const FileWatchEvent& event) override;
    void OnFileChangedAt(const FileWatchEvent& event, TimePoint now);

    std::size_t ProcessDueBackups();
    std::size_t ProcessDueBackupsAt(TimePoint now);

    [[nodiscard]] std::optional<TimePoint> GetNextDueTime() const;
    [[nodiscard]] bool HasPendingBackups() const;

private:
    struct TransparentStringHash {
        using is_transparent = void;

        std::size_t operator()(std::string_view value) const noexcept { return std::hash<std::string_view>{}(value); }

        std::size_t operator()(const std::string& value) const noexcept { return (*this)(std::string_view{value}); }

        std::size_t operator()(const char* value) const noexcept { return (*this)(std::string_view{value}); }
    };

    struct PendingBackup {
        core::TrackedEntry Entry;
        TimePoint DueAt;
    };

    std::optional<core::TrackedEntry> FindTrackedEntry(const std::filesystem::path& path) const;

    std::unordered_map<std::string, core::TrackedEntry, TransparentStringHash, std::equal_to<>> TrackedEntriesByPath_;
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::chrono::milliseconds DebounceDelay_;
    mutable std::mutex Mutex_;
    std::unordered_map<std::string, PendingBackup, TransparentStringHash, std::equal_to<>> PendingBackups_;
};

}  // namespace cfgsync::watch
