#include "watch/EfswFileWatcher.hpp"

#include "Exceptions.hpp"

#include <efsw/efsw.hpp>
#include <filesystem>
#include <fmt/format.h>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cfgsync::watch {
namespace fs = std::filesystem;

namespace {

// The polling backend gives cfgsync deterministic foreground watch behavior across platforms.
constexpr bool UseGenericPollingWatcher = true;
constexpr unsigned int PollingFrequencyMs = 250;

FileWatchAction MapAction(efsw::Action action) {
    using enum FileWatchAction;

    switch (action) {
        case efsw::Actions::Add:
            return Added;
        case efsw::Actions::Delete:
            return Deleted;
        case efsw::Actions::Modified:
            return Modified;
        case efsw::Actions::Moved:
            return Moved;
    }

    throw WatchError{"Received unsupported efsw file action."};
}

fs::path ResolveEventPath(const fs::path& directory, const std::string& filename) {
    fs::path path{filename};
    if (path.is_absolute()) {
        return path.lexically_normal();
    }

    return (directory / path).lexically_normal();
}

std::optional<fs::path> ResolveOldPath(const fs::path& directory, const std::string& oldFilename) {
    if (oldFilename.empty()) {
        return std::nullopt;
    }

    return ResolveEventPath(directory, oldFilename);
}

std::string FormatWatchError(efsw::WatchID watchId, const fs::path& directory) {
    return fmt::format(fmt::runtime("Unable to watch directory '{}': efsw error {}"), directory.string(), watchId);
}

}  // namespace

class EfswFileWatcher::Impl final : public efsw::FileWatchListener {
public:
    Impl() : Watcher_(UseGenericPollingWatcher, PollingFrequencyMs) {
        Watcher_.followSymlinks(false);
        Watcher_.allowOutOfScopeLinks(false);
    }

    ~Impl() override {
        std::vector<efsw::WatchID> watchIds;
        {
            std::scoped_lock lock{Mutex_};
            watchIds = WatchIds_;
            WatchIds_.clear();
            Observers_.clear();
        }

        for (const auto watchId : watchIds) {
            Watcher_.removeWatch(watchId);
        }
    }

    void WatchDirectory(const fs::path& directory, FileWatchObserver& observer) {
        if (directory.empty()) {
            throw WatchError{"Watch directory must not be empty."};
        }

        const auto watchId = Watcher_.addWatch(directory.string(), this, false);
        if (watchId < 0) {
            throw WatchError{FormatWatchError(watchId, directory)};
        }

        std::scoped_lock lock{Mutex_};
        Observers_[watchId] = &observer;
        WatchIds_.push_back(watchId);
    }

    void Start() { Watcher_.watch(); }

    void handleFileAction(efsw::WatchID watchId, const std::string& dir,  // NOLINT(bugprone-easily-swappable-parameters): efsw owns this override signature with adjacent parameters.
                          const std::string& filename,
                          efsw::Action action, const std::string& oldFilename) override {
        FileWatchObserver* observer = nullptr;
        {
            std::scoped_lock lock{Mutex_};
            const auto observerIt = Observers_.find(watchId);
            if (observerIt == Observers_.end()) {
                return;
            }

            observer = observerIt->second;
        }

        const fs::path directory{dir};
        observer->OnFileChanged({
            .Action = MapAction(action),
            .Directory = directory.lexically_normal(),
            .Path = ResolveEventPath(directory, filename),
            .OldPath = ResolveOldPath(directory, oldFilename),
        });
    }

private:
    efsw::FileWatcher Watcher_;
    std::mutex Mutex_;
    std::unordered_map<efsw::WatchID, FileWatchObserver*> Observers_;
    std::vector<efsw::WatchID> WatchIds_;
};

EfswFileWatcher::EfswFileWatcher() : Impl_(std::make_unique<Impl>()) {}

EfswFileWatcher::~EfswFileWatcher() noexcept { Impl_.reset(); }

EfswFileWatcher::EfswFileWatcher(EfswFileWatcher&&) noexcept = default;

EfswFileWatcher& EfswFileWatcher::operator=(EfswFileWatcher&&) noexcept = default;

void EfswFileWatcher::WatchDirectory(const fs::path& directory, FileWatchObserver& observer) {
    Impl_->WatchDirectory(directory, observer);
}

void EfswFileWatcher::Start() { Impl_->Start(); }

}  // namespace cfgsync::watch
