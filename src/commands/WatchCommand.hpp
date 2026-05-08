#pragma once

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"
#include "watch/FileWatcher.hpp"

#include <functional>

namespace cfgsync::commands {

class WatchCommand {
public:
    using StopRequested = std::function<bool()>;

    WatchCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void Execute(watch::FileWatcher& watcher) const;
    void Execute(watch::FileWatcher& watcher, const StopRequested& stopRequested) const;

private:
    core::Registry& Registry_;                 // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
