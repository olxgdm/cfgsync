#include "common/GoogleTestMain.hpp"
#include "common/TestTempDirectory.hpp"
#include "Exceptions.hpp"
#include "gtest/gtest.h"
#include "watch/EfswFileWatcher.hpp"
#include "watch/FileWatcher.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace {
namespace fs = std::filesystem;

class RecordingObserver final : public cfgsync::watch::FileWatchObserver {
public:
    void OnFileChanged(const cfgsync::watch::FileWatchEvent& event) override { Events.push_back(event); }

    std::vector<cfgsync::watch::FileWatchEvent> Events;
};

class FakeFileWatcher final : public cfgsync::watch::FileWatcher {
public:
    void WatchDirectory(const fs::path& directory, cfgsync::watch::FileWatchObserver& observer) override {
        WatchedDirectories.push_back(directory);
        Observer = &observer;
    }

    void Start() override { Started = true; }

    void Emit(const cfgsync::watch::FileWatchEvent& event) {
        ASSERT_NE(Observer, nullptr);
        Observer->OnFileChanged(event);
    }

    bool Started = false;
    std::vector<fs::path> WatchedDirectories;
    cfgsync::watch::FileWatchObserver* Observer = nullptr;
};

TEST(FileWatcherTest, FakeWatcherRecordsWatchedDirectoryAndStartState) {
    FakeFileWatcher watcher;
    RecordingObserver observer;
    const fs::path directory{"/home/user/.config"};

    watcher.WatchDirectory(directory, observer);
    watcher.Start();

    ASSERT_EQ(watcher.WatchedDirectories.size(), 1U);
    EXPECT_EQ(watcher.WatchedDirectories.front(), directory);
    EXPECT_TRUE(watcher.Started);
}

TEST(FileWatcherTest, ObserverReceivesFileChangeEvents) {
    FakeFileWatcher watcher;
    RecordingObserver observer;
    watcher.WatchDirectory("/home/user", observer);

    watcher.Emit({
        .Action = cfgsync::watch::FileWatchAction::Modified,
        .Directory = "/home/user",
        .Path = "/home/user/.gitconfig",
        .OldPath = std::nullopt,
    });

    ASSERT_EQ(observer.Events.size(), 1U);
    EXPECT_EQ(observer.Events.front().Action, cfgsync::watch::FileWatchAction::Modified);
    EXPECT_EQ(observer.Events.front().Directory, fs::path{"/home/user"});
    EXPECT_EQ(observer.Events.front().Path, fs::path{"/home/user/.gitconfig"});
    EXPECT_FALSE(observer.Events.front().OldPath.has_value());
}

TEST(FileWatcherTest, ObserverReceivesMovedEventOldPath) {
    FakeFileWatcher watcher;
    RecordingObserver observer;
    watcher.WatchDirectory("/home/user", observer);

    watcher.Emit({
        .Action = cfgsync::watch::FileWatchAction::Moved,
        .Directory = "/home/user",
        .Path = "/home/user/.gitconfig",
        .OldPath = fs::path{"/home/user/.gitconfig.old"},
    });

    ASSERT_EQ(observer.Events.size(), 1U);
    EXPECT_EQ(observer.Events.front().Action, cfgsync::watch::FileWatchAction::Moved);
    ASSERT_TRUE(observer.Events.front().OldPath.has_value());
    EXPECT_EQ(observer.Events.front().OldPath.value(), fs::path{"/home/user/.gitconfig.old"});
}

TEST(FileWatcherTest, EfswWatcherConstructsMovesAndRejectsEmptyDirectory) {
    cfgsync::watch::EfswFileWatcher watcher;
    cfgsync::watch::EfswFileWatcher movedWatcher{std::move(watcher)};
    RecordingObserver observer;

    EXPECT_THROW(movedWatcher.WatchDirectory({}, observer), cfgsync::WatchError);

    cfgsync::watch::EfswFileWatcher assignedWatcher;
    assignedWatcher = std::move(movedWatcher);

    EXPECT_THROW(assignedWatcher.WatchDirectory({}, observer), cfgsync::WatchError);
}

TEST(FileWatcherTest, EfswWatcherWatchesExistingDirectoryAndCleansUpOnDestruction) {
    const auto directory = cfgsync::tests::MakeTestRoot();
    RecordingObserver observer;

    {
        cfgsync::watch::EfswFileWatcher watcher;
        EXPECT_NO_THROW(watcher.WatchDirectory(directory, observer));
    }

    fs::remove_all(directory);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
