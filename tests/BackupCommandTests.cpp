#include "commands/BackupCommand.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "storage/StorageManager.hpp"

#include "gtest/gtest.h"

#include <filesystem>
#include <stdexcept>
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

TEST_F(BackupCommandTest, OverwritesExistingStoredCopy) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "new contents\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "old contents\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / storedRelativePath), "new contents\n");
}

TEST_F(BackupCommandTest, ContinuesAfterMissingSourceAndReportsPartialFailure) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingPath = SourcePath("missing.conf");
    cfgsync::tests::WriteTextFile(existingPath, "[user]\n");
    const auto existingStoredRelativePath = TrackFile(Registry(), existingPath);
    TrackFile(Registry(), missingPath);
    const auto registryBeforeBackup = cfgsync::tests::ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    try {
        command.Execute();
        FAIL() << "Backup with a missing source did not throw.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Backup completed with 1 failure."), std::string::npos);
    }

    EXPECT_EQ(cfgsync::tests::ReadTextFile(StorageRoot() / existingStoredRelativePath), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeBackup);
}

TEST_F(BackupCommandTest, EmptyRegistrySucceedsWithoutCreatingStoredFiles) {
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    EXPECT_NO_THROW(command.Execute());
    EXPECT_FALSE(fs::exists(StorageRoot() / "files"));
}

}  // namespace

int main(int argc, char** argv) {
    return cfgsync::tests::RunGoogleTests(argc, argv);
}
