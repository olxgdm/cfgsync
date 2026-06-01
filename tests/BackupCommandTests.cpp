#include "Exceptions.hpp"
#include "commands/BackupCommand.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "gtest/gtest.h"
#include "storage/StorageManager.hpp"

#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;
using cfgsync::tests::TrackFile;

class BackupCommandTest : public cfgsync::tests::RegistryCommandTestFixture {};

TEST_F(BackupCommandTest, BacksUpOneTrackedFile) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "[user]\n");
}

TEST_F(BackupCommandTest, BacksUpMultipleTrackedFiles) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("init.lua");
    cfgsync::tests::WriteTextFile(firstPath, "[user]\n");
    cfgsync::tests::WriteTextFile(secondPath, "vim.opt.number = true\n");
    const auto firstStoredRelativePath = TrackFile(Registry(), firstPath);
    const auto secondStoredRelativePath = TrackFile(Registry(), secondPath);

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / firstStoredRelativePath), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / secondStoredRelativePath), "vim.opt.number = true\n");
}

TEST_F(BackupCommandTest, CreatesDestinationParentDirectories) {
    const auto sourcePath = SourcePath(".config/nvim/init.lua");
    cfgsync::tests::WriteTextFile(sourcePath, "vim.opt.number = true\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    ASSERT_FALSE(fs::exists((StorageRoot() / storedRelativePath).parent_path()));

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_TRUE(fs::exists(StorageRoot() / storedRelativePath));
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "vim.opt.number = true\n");
}

TEST_F(BackupCommandTest, SkipsExistingStoredCopyWhenContentMatches) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "same contents\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "same contents\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    testing::internal::CaptureStdout();
    command.Execute();
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "same contents\n");
    EXPECT_NE(output.find("No new files to back up."), std::string::npos);
}

TEST_F(BackupCommandTest, RefreshesExistingStoredCopyWhenContentDiffers) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "new contents\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "old contents\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "new contents\n");
}

TEST_F(BackupCommandTest, SkipsCleanRefreshesChangedAndCreatesMissingStoredCopies) {
    const auto cleanBackupPath = SourcePath(".gitconfig");
    const auto changedBackupPath = SourcePath("starship.toml");
    const auto missingBackupPath = SourcePath("init.lua");
    cfgsync::tests::WriteTextFile(cleanBackupPath, "clean contents\n");
    cfgsync::tests::WriteTextFile(changedBackupPath, "changed contents\n");
    cfgsync::tests::WriteTextFile(missingBackupPath, "vim.opt.number = true\n");
    const auto cleanStoredRelativePath = TrackFile(Registry(), cleanBackupPath);
    const auto changedStoredRelativePath = TrackFile(Registry(), changedBackupPath);
    const auto missingStoredRelativePath = TrackFile(Registry(), missingBackupPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / cleanStoredRelativePath, "clean contents\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / changedStoredRelativePath, "stored contents\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / cleanStoredRelativePath), "clean contents\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / changedStoredRelativePath), "changed contents\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / missingStoredRelativePath), "vim.opt.number = true\n");
}

TEST_F(BackupCommandTest, ContinuesAfterMissingSourceAndReportsPartialFailure) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto changedPath = SourcePath("starship.toml");
    const auto missingPath = SourcePath("missing.conf");
    cfgsync::tests::WriteTextFile(existingPath, "[user]\n");
    cfgsync::tests::WriteTextFile(changedPath, "changed contents\n");
    const auto existingStoredRelativePath = TrackFile(Registry(), existingPath);
    const auto changedStoredRelativePath = TrackFile(Registry(), changedPath);
    TrackFile(Registry(), missingPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / changedStoredRelativePath, "stored contents\n");
    const auto registryBeforeBackup = cfgsync::tests::ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    try {
        command.Execute();
        FAIL() << "Backup with a missing source did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Backup completed with 1 failure."), std::string::npos);
    }

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / existingStoredRelativePath), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / changedStoredRelativePath), "changed contents\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeBackup);
}

TEST_F(BackupCommandTest, MissingSourcesWithExistingBackupsReportPluralFailureAndLeaveBackupsUntouched) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("starship.toml");
    cfgsync::tests::WriteTextFile(firstPath, "first current\n");
    cfgsync::tests::WriteTextFile(secondPath, "second current\n");
    const auto firstStoredRelativePath = TrackFile(Registry(), firstPath);
    const auto secondStoredRelativePath = TrackFile(Registry(), secondPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / firstStoredRelativePath, "first stored\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / secondStoredRelativePath, "second stored\n");
    fs::remove(firstPath);
    fs::remove(secondPath);

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    try {
        command.Execute();
        FAIL() << "Backup with missing sources and existing backups did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Backup completed with 2 failures."), std::string::npos);
    }

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / firstStoredRelativePath), "first stored\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / secondStoredRelativePath), "second stored\n");
}

TEST_F(BackupCommandTest, EmptyRegistrySucceedsWithoutCreatingStoredFiles) {
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    EXPECT_NO_THROW(command.Execute());
    EXPECT_FALSE(fs::exists(StorageRoot() / "files"));
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
