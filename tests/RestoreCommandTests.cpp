#include "Exceptions.hpp"
#include "commands/RestoreCommand.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "gtest/gtest.h"
#include "storage/StorageManager.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;
using cfgsync::tests::TrackFile;

class RestoreCommandTest : public cfgsync::tests::RegistryCommandTestFixture {};

TEST_F(RestoreCommandTest, RestoresOneTrackedFile) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(sourcePath, "changed contents\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteSingle(sourcePath);

    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "[user]\n");
}

TEST_F(RestoreCommandTest, RestoresAllTrackedFiles) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("init.lua");
    const auto firstStoredRelativePath = TrackFile(Registry(), firstPath);
    const auto secondStoredRelativePath = TrackFile(Registry(), secondPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / firstStoredRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / secondStoredRelativePath, "vim.opt.number = true\n");
    cfgsync::tests::WriteTextFile(firstPath, "changed first\n");
    cfgsync::tests::WriteTextFile(secondPath, "changed second\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteAll();

    EXPECT_EQ(cfgsync::tests::ReadTextFile(firstPath), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(secondPath), "vim.opt.number = true\n");
}

TEST_F(RestoreCommandTest, CreatesDestinationParentDirectories) {
    const auto sourcePath = SourcePath(".config/nvim/init.lua");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "vim.opt.number = true\n");
    ASSERT_FALSE(fs::exists(sourcePath.parent_path()));

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteSingle(sourcePath);

    EXPECT_TRUE(fs::exists(sourcePath));
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "vim.opt.number = true\n");
}

TEST_F(RestoreCommandTest, OverwritesChangedLocalFile) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteSingle(sourcePath);

    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "stored contents\n");
}

TEST_F(RestoreCommandTest, SingleRestoreFailsForUntrackedFile) {
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteSingle(SourcePath());
        FAIL() << "Restore of an untracked file did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("File is not tracked"), std::string::npos);
        EXPECT_NE(message.find(cfgsync::utils::NormalizePath(SourcePath()).string()), std::string::npos);
    }
}

TEST_F(RestoreCommandTest, SingleRestoreFailsWhenStoredBackupIsMissing) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteSingle(sourcePath);
        FAIL() << "Restore with a missing stored backup did not throw.";
    } catch (const cfgsync::FileError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Path does not exist"), std::string::npos);
        EXPECT_NE(message.find((StorageRoot() / storedRelativePath.generic_string()).string()), std::string::npos);
    }
}

TEST_F(RestoreCommandTest, RestoreAllContinuesAfterMissingStoredBackupAndReportsPartialFailure) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingBackupPath = SourcePath("missing.conf");
    const auto existingStoredRelativePath = TrackFile(Registry(), existingPath);
    TrackFile(Registry(), missingBackupPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / existingStoredRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(existingPath, "changed contents\n");
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteAll();
        FAIL() << "Restore with a missing stored backup did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Restore completed with 1 failure."), std::string::npos);
    }

    EXPECT_EQ(cfgsync::tests::ReadTextFile(existingPath), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeRestore);
}

TEST_F(RestoreCommandTest, EmptyRegistrySucceedsWithoutCreatingStoredFiles) {
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    EXPECT_NO_THROW(command.ExecuteAll());
    EXPECT_FALSE(fs::exists(StorageRoot() / "files"));
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
