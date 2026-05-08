#pragma once

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"
#include "watch/FileWatcher.hpp"

namespace cfgsync::commands {

class WatchCommand {
public:
    WatchCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void Execute(watch::FileWatcher& watcher) const;

    template <typename StopRequested>
    void Execute(watch::FileWatcher& watcher, const StopRequested& stopRequested) const {
        class StopRequestedAdapter final : public StopRequestedCallback {
        public:
            explicit StopRequestedAdapter(const StopRequested& callback) : Callback_(callback) {}

            bool IsStopRequested() const override { return Callback_(); }

        private:
            const StopRequested& Callback_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
        };

        const StopRequestedAdapter adapter{stopRequested};
        ExecuteWithStopRequested(watcher, adapter);
    }

private:
    class StopRequestedCallback {
    public:
        virtual ~StopRequestedCallback() = default;
        virtual bool IsStopRequested() const = 0;
    };

    void ExecuteWithStopRequested(watch::FileWatcher& watcher, const StopRequestedCallback& stopRequested) const;

    core::Registry& Registry_;                 // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
