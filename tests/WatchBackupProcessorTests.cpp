#include "common/GoogleTestMain.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "gtest/gtest.h"
#include "storage/StorageManager.hpp"
#include "watch/WatchBackupProcessor.hpp"

#include <chrono>
#include <filesystem>
#include <optional>

namespace {
namespace fs = std::filesystem;
using cfgsync::tests::TrackFile;
using cfgsync::watch::FileWatchAction;
using cfgsync::watch::WatchBackupProcessor;

class WatchBackupProcessorTest : public cfgsync::tests::RegistryCommandTestFixture {
protected:
    static WatchBackupProcessor::TimePoint At(std::chrono::milliseconds offset) {
        return WatchBackupProcessor::TimePoint{offset};
    }

    static cfgsync::watch::FileWatchEvent Event(FileWatchAction action, const fs::path& path) {
        return {
            .Action = action,
            .Directory = path.parent_path(),
            .Path = path,
            .OldPath = std::nullopt,
        };
    }
};

TEST_F(WatchBackupProcessorTest, BacksUpTrackedModifiedFileAfterDebounceDelay) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "changed\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    WatchBackupProcessor processor{Registry().GetTrackedEntries(), storageManager};

    processor.OnFileChangedAt(Event(FileWatchAction::Modified, sourcePath), At(std::chrono::milliseconds{0}));

    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{499})), 0U);
    EXPECT_FALSE(fs::exists(StorageRoot() / storedRelativePath));

    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{500})), 1U);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "changed\n");
}

TEST_F(WatchBackupProcessorTest, IgnoresUntrackedFileEvents) {
    const auto trackedPath = SourcePath(".gitconfig");
    const auto untrackedPath = SourcePath("untracked.conf");
    cfgsync::tests::WriteTextFile(trackedPath, "tracked\n");
    cfgsync::tests::WriteTextFile(untrackedPath, "untracked\n");
    TrackFile(Registry(), trackedPath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    WatchBackupProcessor processor{Registry().GetTrackedEntries(), storageManager};

    processor.OnFileChangedAt(Event(FileWatchAction::Modified, untrackedPath), At(std::chrono::milliseconds{0}));

    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{500})), 0U);
    EXPECT_FALSE(processor.HasPendingBackups());
}

TEST_F(WatchBackupProcessorTest, DebouncesDuplicateTrackedEvents) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "first\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    WatchBackupProcessor processor{Registry().GetTrackedEntries(), storageManager};

    processor.OnFileChangedAt(Event(FileWatchAction::Modified, sourcePath), At(std::chrono::milliseconds{0}));
    cfgsync::tests::WriteTextFile(sourcePath, "second\n");
    processor.OnFileChangedAt(Event(FileWatchAction::Modified, sourcePath), At(std::chrono::milliseconds{100}));

    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{500})), 0U);
    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{600})), 1U);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "second\n");
}

TEST_F(WatchBackupProcessorTest, DebouncesDifferentFilesIndependently) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("tool.conf");
    cfgsync::tests::WriteTextFile(firstPath, "first\n");
    cfgsync::tests::WriteTextFile(secondPath, "second\n");
    const auto firstStoredPath = TrackFile(Registry(), firstPath);
    const auto secondStoredPath = TrackFile(Registry(), secondPath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    WatchBackupProcessor processor{Registry().GetTrackedEntries(), storageManager};

    processor.OnFileChangedAt(Event(FileWatchAction::Modified, firstPath), At(std::chrono::milliseconds{0}));
    processor.OnFileChangedAt(Event(FileWatchAction::Modified, secondPath), At(std::chrono::milliseconds{300}));

    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{500})), 1U);
    EXPECT_TRUE(fs::exists(StorageRoot() / firstStoredPath));
    EXPECT_FALSE(fs::exists(StorageRoot() / secondStoredPath));

    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{800})), 1U);
    EXPECT_TRUE(fs::exists(StorageRoot() / secondStoredPath));
}

TEST_F(WatchBackupProcessorTest, DeleteEventWarnsWithoutSchedulingBackup) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "tracked\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    WatchBackupProcessor processor{Registry().GetTrackedEntries(), storageManager};

    processor.OnFileChangedAt(Event(FileWatchAction::Deleted, sourcePath), At(std::chrono::milliseconds{0}));

    EXPECT_FALSE(processor.HasPendingBackups());
    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{500})), 0U);
    EXPECT_FALSE(fs::exists(StorageRoot() / storedRelativePath));
}

TEST_F(WatchBackupProcessorTest, MovedEventBacksUpWhenNewPathIsTracked) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "moved into place\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    WatchBackupProcessor processor{Registry().GetTrackedEntries(), storageManager};

    processor.OnFileChangedAt(
        {
            .Action = FileWatchAction::Moved,
            .Directory = sourcePath.parent_path(),
            .Path = sourcePath,
            .OldPath = sourcePath.parent_path() / ".gitconfig.tmp",
        },
        At(std::chrono::milliseconds{0}));

    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{500})), 1U);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "moved into place\n");
}

TEST_F(WatchBackupProcessorTest, ContinuesAfterRecoverableBackupFailure) {
    const auto missingPath = SourcePath("missing.conf");
    const auto existingPath = SourcePath("existing.conf");
    cfgsync::tests::WriteTextFile(missingPath, "temporary\n");
    cfgsync::tests::WriteTextFile(existingPath, "existing\n");
    TrackFile(Registry(), missingPath);
    const auto existingStoredPath = TrackFile(Registry(), existingPath);
    fs::remove(missingPath);
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    WatchBackupProcessor processor{Registry().GetTrackedEntries(), storageManager};

    processor.OnFileChangedAt(Event(FileWatchAction::Modified, missingPath), At(std::chrono::milliseconds{0}));
    processor.OnFileChangedAt(Event(FileWatchAction::Modified, existingPath), At(std::chrono::milliseconds{0}));

    EXPECT_EQ(processor.ProcessDueBackupsAt(At(std::chrono::milliseconds{500})), 2U);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / existingStoredPath), "existing\n");
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
