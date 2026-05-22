#include "Exceptions.hpp"
#include "commands/WatchCommand.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "gtest/gtest.h"
#include "utils/PathUtils.hpp"
#include "watch/FileWatchEvent.hpp"
#include "watch/FileWatcher.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;
using cfgsync::tests::TrackFile;
using cfgsync::watch::FileWatchAction;

struct TransparentStringHash {
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept { return std::hash<std::string_view>{}(value); }

    std::size_t operator()(const std::string& value) const noexcept { return (*this)(std::string_view{value}); }

    std::size_t operator()(const char* value) const noexcept { return (*this)(std::string_view{value}); }
};

class FakeFileWatcher final : public cfgsync::watch::FileWatcher {
public:
    void WatchDirectory(const fs::path& directory, cfgsync::watch::FileWatchObserver& observer) override {
        if (FailingDirectories.contains(directory.string())) {
            throw cfgsync::WatchError{"Unable to watch directory '" + directory.string() + "'"};
        }

        WatchedDirectories.push_back(directory);
        Observer = &observer;
    }

    void Start() override {
        Started = true;
        if (Observer == nullptr) {
            return;
        }

        for (const auto& action : StartActions) {
            action(*Observer);
        }
    }

    void EmitOnStart(cfgsync::watch::FileWatchEvent event) {
        StartActions.push_back(
            [event = std::move(event)](cfgsync::watch::FileWatchObserver& observer) { observer.OnFileChanged(event); });
    }

    bool Started = false;
    std::vector<fs::path> WatchedDirectories;
    cfgsync::watch::FileWatchObserver* Observer = nullptr;
    std::vector<std::function<void(cfgsync::watch::FileWatchObserver&)>> StartActions;
    std::unordered_set<std::string, TransparentStringHash, std::equal_to<>> FailingDirectories;
};

class WatchCommandTest : public cfgsync::tests::RegistryCommandTestFixture {
protected:
    static bool StopImmediately() { return true; }

    static cfgsync::watch::FileWatchEvent Event(FileWatchAction action, const fs::path& path) {
        return {
            .Action = action,
            .Directory = path.parent_path(),
            .Path = path,
            .OldPath = std::nullopt,
        };
    }

    void AddMissingTrackedPath(const fs::path& sourcePath) {
        const auto normalizedPath = cfgsync::utils::NormalizePath(sourcePath);
        const auto storedRelativePath = cfgsync::utils::MakeStorageRelativePath(normalizedPath);
        ASSERT_TRUE(Registry().AddEntry({
            .OriginalPath = normalizedPath.string(),
            .StoredRelativePath = storedRelativePath.generic_string(),
        }));
        Registry().Save();
    }
};

TEST_F(WatchCommandTest, EmptyRegistrySucceedsWithoutStartingWatcher) {
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager};
    FakeFileWatcher watcher;

    command.Execute(watcher, StopImmediately);

    EXPECT_FALSE(watcher.Started);
    EXPECT_TRUE(watcher.WatchedDirectories.empty());
}

TEST_F(WatchCommandTest, DeduplicatesTrackedParentDirectories) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("tool.conf");
    cfgsync::tests::WriteTextFile(firstPath, "first\n");
    cfgsync::tests::WriteTextFile(secondPath, "second\n");
    TrackFile(Registry(), firstPath);
    TrackFile(Registry(), secondPath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager};
    FakeFileWatcher watcher;

    command.Execute(watcher, StopImmediately);

    EXPECT_TRUE(watcher.Started);
    ASSERT_EQ(watcher.WatchedDirectories.size(), 1U);
    EXPECT_EQ(watcher.WatchedDirectories.front(), firstPath.parent_path());
}

TEST_F(WatchCommandTest, SkipsTrackedFilesWhoseParentDirectoryIsMissing) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingPath = StorageRoot().parent_path() / "missing-parent" / "tool.conf";
    cfgsync::tests::WriteTextFile(existingPath, "first\n");
    TrackFile(Registry(), existingPath);
    AddMissingTrackedPath(missingPath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager};
    FakeFileWatcher watcher;

    command.Execute(watcher, StopImmediately);

    ASSERT_EQ(watcher.WatchedDirectories.size(), 1U);
    EXPECT_EQ(watcher.WatchedDirectories.front(), existingPath.parent_path());
}

TEST_F(WatchCommandTest, FailsWhenNoTrackedParentDirectoriesCanBeWatched) {
    AddMissingTrackedPath(StorageRoot().parent_path() / "missing-parent" / "tool.conf");
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager};
    FakeFileWatcher watcher;

    try {
        command.Execute(watcher, StopImmediately);
        FAIL() << "Watch command did not fail when no directories could be watched.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("no tracked file parent directories could be watched"), std::string::npos);
    }
}

TEST_F(WatchCommandTest, ContinuesWhenOneWatchDirectoryFails) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = StorageRoot().parent_path() / "other" / "tool.conf";
    cfgsync::tests::WriteTextFile(firstPath, "first\n");
    cfgsync::tests::WriteTextFile(secondPath, "second\n");
    TrackFile(Registry(), firstPath);
    TrackFile(Registry(), secondPath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager};
    FakeFileWatcher watcher;
    watcher.FailingDirectories.insert(firstPath.parent_path().string());

    command.Execute(watcher, StopImmediately);

    EXPECT_TRUE(watcher.Started);
    ASSERT_EQ(watcher.WatchedDirectories.size(), 1U);
    EXPECT_EQ(watcher.WatchedDirectories.front(), secondPath.parent_path());
}

TEST_F(WatchCommandTest, DoesNotBackUpTrackedFilesOnStartupWithoutEvents) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "tracked\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager, std::chrono::milliseconds{0}};
    FakeFileWatcher watcher;

    command.Execute(watcher, StopImmediately);

    EXPECT_TRUE(watcher.Started);
    EXPECT_FALSE(fs::exists(StorageRoot() / storedRelativePath));
}

TEST_F(WatchCommandTest, BacksUpTrackedModifiedEventThroughObserverPath) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "changed\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager, std::chrono::milliseconds{0}};
    FakeFileWatcher watcher;
    watcher.EmitOnStart(Event(FileWatchAction::Modified, sourcePath));

    command.Execute(watcher, StopImmediately);

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "changed\n");
}

TEST_F(WatchCommandTest, DebouncesDuplicateTrackedEventsBeforeProcessing) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "first\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager, std::chrono::milliseconds{0}};
    FakeFileWatcher watcher;
    watcher.StartActions.push_back([&sourcePath](cfgsync::watch::FileWatchObserver& observer) {
        observer.OnFileChanged(Event(FileWatchAction::Modified, sourcePath));
        cfgsync::tests::WriteTextFile(sourcePath, "second\n");
        observer.OnFileChanged(Event(FileWatchAction::Modified, sourcePath));
    });

    command.Execute(watcher, StopImmediately);

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "second\n");
}

TEST_F(WatchCommandTest, IgnoresUntrackedEventsThroughObserverPath) {
    const auto trackedPath = SourcePath(".gitconfig");
    const auto untrackedPath = SourcePath("tool.conf");
    cfgsync::tests::WriteTextFile(trackedPath, "tracked\n");
    cfgsync::tests::WriteTextFile(untrackedPath, "untracked\n");
    const auto storedRelativePath = TrackFile(Registry(), trackedPath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager, std::chrono::milliseconds{0}};
    FakeFileWatcher watcher;
    watcher.EmitOnStart(Event(FileWatchAction::Modified, untrackedPath));

    command.Execute(watcher, StopImmediately);

    EXPECT_FALSE(fs::exists(StorageRoot() / storedRelativePath));
}

TEST_F(WatchCommandTest, ProcessesAlreadyDueBackupsWhenStopRequested) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "added\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager, std::chrono::milliseconds{0}};
    FakeFileWatcher watcher;
    watcher.EmitOnStart(Event(FileWatchAction::Added, sourcePath));

    command.Execute(watcher, StopImmediately);

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "added\n");
}

TEST_F(WatchCommandTest, DoesNotWaitForFutureDebounceBackupsWhenStopRequested) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "future\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::WatchCommand command{Registry(), storageManager};
    FakeFileWatcher watcher;
    watcher.EmitOnStart(Event(FileWatchAction::Moved, sourcePath));

    command.Execute(watcher, StopImmediately);

    EXPECT_FALSE(fs::exists(StorageRoot() / storedRelativePath));
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
