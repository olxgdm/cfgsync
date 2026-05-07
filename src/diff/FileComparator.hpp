#pragma once

#include "core/TrackedEntry.hpp"
#include "storage/StorageManager.hpp"

#include <cstdint>
#include <vector>

namespace cfgsync::diff {

enum class FileStatus : std::uint8_t {
    Clean,
    Modified,
    MissingOriginal,
    MissingBackup,
};

struct FileStatusResult {
    FileStatus Status;
    core::TrackedEntry Entry;
};

class FileComparator {
public:
    explicit FileComparator(const storage::StorageManager& storageManager);

    FileStatusResult Compare(const core::TrackedEntry& entry) const;
    std::vector<FileStatusResult> CompareAll(const std::vector<core::TrackedEntry>& entries) const;

private:
    const storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::diff
