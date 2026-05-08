#include "Exceptions.hpp"
#include "commands/WatchCommand.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "gtest/gtest.h"
#include "utils/PathUtils.hpp"
#include "watch/FileWatcher.hpp"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
namespace fs = std::filesystem;
using cfgsync::tests::TrackFile;

class FakeFileWatcher final : public cfgsync::watch::FileWatcher {
public:
    void WatchDirectory(const fs::path& directory, cfgsync::watch::FileWatchObserver& observer) override {
        if (FailingDirectories.contains(directory.string())) {
            throw cfgsync::WatchError{"Unable to watch directory '" + directory.string() + "'"};
        }

        WatchedDirectories.push_back(directory);
        Observer = &observer;
    }

    void Start() override { Started = true; }

    bool Started = false;
    std::vector<fs::path> WatchedDirectories;
    cfgsync::watch::FileWatchObserver* Observer = nullptr;
    std::unordered_set<std::string> FailingDirectories;
};

class WatchCommandTest : public cfgsync::tests::RegistryCommandTestFixture {
protected:
    static bool StopImmediately() { return true; }

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

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
