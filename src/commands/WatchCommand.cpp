#include "commands/WatchCommand.hpp"

#include "Exceptions.hpp"
#include "utils/LogUtils.hpp"
#include "watch/WatchBackupProcessor.hpp"

#include <chrono>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <vector>

namespace cfgsync::commands {
namespace fs = std::filesystem;

namespace {

constexpr auto PollInterval = std::chrono::milliseconds{50};

struct TransparentStringHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept { return std::hash<std::string_view>{}(value); }

    std::size_t operator()(const std::string& value) const noexcept { return (*this)(std::string_view{value}); }

    std::size_t operator()(const char* value) const noexcept { return (*this)(std::string_view{value}); }
};

volatile std::sig_atomic_t& StopSignalReceived() {
    static volatile std::sig_atomic_t signalReceived = 0;
    return signalReceived;
}

void HandleStopSignal(int /*signal*/) { StopSignalReceived() = 1; }

class ScopedStopSignalHandlers {
public:
    ScopedStopSignalHandlers() {
        StopSignalReceived() = 0;
        PreviousIntHandler_ = std::signal(SIGINT, HandleStopSignal);
        PreviousTermHandler_ = std::signal(SIGTERM, HandleStopSignal);
    }

    ~ScopedStopSignalHandlers() {
        std::signal(SIGINT, PreviousIntHandler_);
        std::signal(SIGTERM, PreviousTermHandler_);
    }

    ScopedStopSignalHandlers(const ScopedStopSignalHandlers&) = delete;
    ScopedStopSignalHandlers& operator=(const ScopedStopSignalHandlers&) = delete;

private:
    using SignalHandler = void (*)(int);

    SignalHandler PreviousIntHandler_ = SIG_DFL;
    SignalHandler PreviousTermHandler_ = SIG_DFL;
};

bool SignalStopRequested() { return StopSignalReceived() != 0; }

bool IsExistingDirectory(const fs::path& path) {
    std::error_code errorCode;
    const auto status = fs::status(path, errorCode);
    return !errorCode && fs::exists(status) && fs::is_directory(status);
}

std::vector<core::TrackedEntry> CopyTrackedEntries(const core::Registry& registry) {
    return registry.GetTrackedEntries();
}

std::vector<fs::path> CollectWatchDirectories(const std::vector<core::TrackedEntry>& trackedEntries) {
    std::vector<fs::path> directories;
    std::unordered_set<std::string, TransparentStringHash, std::equal_to<>> seenDirectories;

    for (const auto& entry : trackedEntries) {
        const fs::path originalPath{entry.OriginalPath};
        const auto parentDirectory = originalPath.parent_path().lexically_normal();
        if (!IsExistingDirectory(parentDirectory)) {
            utils::LogWarn(std::format("Tracked file parent directory does not exist: {}", entry.OriginalPath));
            continue;
        }

        const auto directoryKey = parentDirectory.string();
        if (seenDirectories.insert(directoryKey).second) {
            directories.push_back(parentDirectory);
        }
    }

    return directories;
}

std::chrono::milliseconds SleepDurationUntil(std::optional<watch::WatchBackupProcessor::TimePoint> nextDueTime) {
    if (!nextDueTime.has_value()) {
        return PollInterval;
    }

    const auto now = watch::WatchBackupProcessor::Clock::now();
    if (nextDueTime.value() <= now) {
        return std::chrono::milliseconds{0};
    }

    return std::min(PollInterval, std::chrono::duration_cast<std::chrono::milliseconds>(nextDueTime.value() - now));
}

}  // namespace

WatchCommand::WatchCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void WatchCommand::Execute(watch::FileWatcher& watcher) const {
    const ScopedStopSignalHandlers signalHandlers;
    Execute(watcher, SignalStopRequested);
}

void WatchCommand::ExecuteWithStopRequested(watch::FileWatcher& watcher,
                                            const StopRequestedCallback& stopRequested) const {
    const auto trackedEntries = CopyTrackedEntries(Registry_);
    if (trackedEntries.empty()) {
        utils::LogInfo("No files tracked.");
        return;
    }

    watch::WatchBackupProcessor processor{trackedEntries, StorageManager_};
    const auto watchDirectories = CollectWatchDirectories(trackedEntries);

    std::size_t startedWatchCount = 0;
    for (const auto& directory : watchDirectories) {
        try {
            watcher.WatchDirectory(directory, processor);
            ++startedWatchCount;
        } catch (const WatchError& error) {
            utils::LogWarn(error.what());
        }
    }

    if (startedWatchCount == 0) {
        throw CommandError{"Unable to start cfgsync watch: no tracked file parent directories could be watched."};
    }

    utils::LogInfo(std::format("Watching {} tracked files. Press Ctrl+C to stop.", trackedEntries.size()));
    watcher.Start();

    while (!stopRequested.IsStopRequested()) {
        processor.ProcessDueBackups();
        const auto sleepDuration = SleepDurationUntil(processor.GetNextDueTime());
        if (sleepDuration > std::chrono::milliseconds{0}) {
            std::this_thread::sleep_for(sleepDuration);
        }
    }

    utils::LogInfo("Stopped watching.");
}

}  // namespace cfgsync::commands
