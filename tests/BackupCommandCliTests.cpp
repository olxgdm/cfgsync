#include "common/CliCommandTestFixture.hpp"
#include "common/CliTestUtils.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/TestFileUtils.hpp"
#include "gtest/gtest.h"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;

class BackupCommandCliTest : public cfgsync::tests::CliCommandTestFixture {
protected:
    cfgsync::tests::CommandResult RunBackupCommand() const { return RunCommand("backup"); }
};

TEST_F(BackupCommandCliTest, BackupUsesActiveStorageRootPersistedByInit) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));

    const auto result = RunBackupCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("Backed up file"), std::string::npos);
    EXPECT_NE(result.Output.find(cfgsync::utils::NormalizePath(sourcePath).string()), std::string::npos);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(cfgsync::tests::ReadTextFile(cfgsync::tests::StoredPathFor(StorageRoot(), sourcePath)), "[user]\n");
}

TEST_F(BackupCommandCliTest, BackupCopiesMultipleTrackedFiles) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("init.lua");
    cfgsync::tests::WriteTextFile(firstPath, "[user]\n");
    cfgsync::tests::WriteTextFile(secondPath, "vim.opt.number = true\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(firstPath));
    ASSERT_TRUE(RunAddCommand(secondPath));

    const auto result = RunBackupCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(cfgsync::tests::StoredPathFor(StorageRoot(), firstPath)), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(cfgsync::tests::StoredPathFor(StorageRoot(), secondPath)),
              "vim.opt.number = true\n");
}

TEST_F(BackupCommandCliTest, BackupOverwritesExistingStoredCopy) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "new contents\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    cfgsync::tests::WriteTextFile(cfgsync::tests::StoredPathFor(StorageRoot(), sourcePath), "old contents\n");

    const auto result = RunBackupCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(cfgsync::tests::StoredPathFor(StorageRoot(), sourcePath)), "new contents\n");
}

TEST_F(BackupCommandCliTest, BackupContinuesAfterMissingSourceAndReturnsNonZero) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingPath = SourcePath("missing.conf");
    cfgsync::tests::WriteTextFile(existingPath, "[user]\n");
    cfgsync::tests::WriteTextFile(missingPath, "temporary\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(existingPath));
    ASSERT_TRUE(RunAddCommand(missingPath));
    const auto registryBeforeBackup = cfgsync::tests::ReadJsonFile(StorageRoot() / "registry.json");
    fs::remove(missingPath);

    const auto result = RunBackupCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("Backed up file"), std::string::npos);
    EXPECT_NE(result.Output.find("Failed to back up file"), std::string::npos);
    EXPECT_NE(result.Output.find(cfgsync::utils::NormalizePath(missingPath).string()), std::string::npos);
    EXPECT_NE(result.Error.find("Backup completed with 1 failure."), std::string::npos);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(cfgsync::tests::StoredPathFor(StorageRoot(), existingPath)), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(StorageRoot() / "registry.json"), registryBeforeBackup);
}

TEST_F(BackupCommandCliTest, BackupOfEmptyRegistrySucceedsClearly) {
    ASSERT_TRUE(RunInitCommand());

    const auto result = RunBackupCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("No files tracked."), std::string::npos);
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(BackupCommandCliTest, MissingAppConfigFailsWithInitGuidance) {
    const auto result = RunBackupCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("cfgsync has not been initialized"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync init --storage <dir>"), std::string::npos);
}

TEST_F(BackupCommandCliTest, MalformedRegistryFailsClearly) {
    ASSERT_TRUE(RunInitCommand());
    cfgsync::tests::WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result = RunBackupCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Malformed cfgsync registry"), std::string::npos);
}

}  // namespace

int main(int argc, char** argv) {
    return cfgsync::tests::RunCfgsyncCliGoogleTests(argc, argv);
}
