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

class RestoreCommandCliTest : public cfgsync::tests::CliCommandTestFixture {
protected:
    cfgsync::tests::CommandResult RunBackupCommand() const { return RunCommand("backup"); }

    cfgsync::tests::CommandResult RunRestoreAllCommand() const { return RunCommand("restore --all"); }

    cfgsync::tests::CommandResult RunRestoreSingleCommand(const fs::path& sourcePath) const {
        return RunCommand("restore " + cfgsync::tests::QuoteForCommand(sourcePath));
    }
};

TEST_F(RestoreCommandCliTest, RestoreSingleUsesActiveStorageRootPersistedByInit) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    cfgsync::tests::WriteTextFile(sourcePath, "changed contents\n");

    const auto result = RunRestoreSingleCommand(sourcePath);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("Restored file"), std::string::npos);
    EXPECT_NE(result.Output.find(cfgsync::utils::NormalizePath(sourcePath).string()), std::string::npos);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "[user]\n");
}

TEST_F(RestoreCommandCliTest, RestoreAllRestoresMultipleTrackedFiles) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("init.lua");
    cfgsync::tests::WriteTextFile(firstPath, "[user]\n");
    cfgsync::tests::WriteTextFile(secondPath, "vim.opt.number = true\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(firstPath));
    ASSERT_TRUE(RunAddCommand(secondPath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    cfgsync::tests::WriteTextFile(firstPath, "changed first\n");
    cfgsync::tests::WriteTextFile(secondPath, "changed second\n");

    const auto result = RunRestoreAllCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(firstPath), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(secondPath), "vim.opt.number = true\n");
}

TEST_F(RestoreCommandCliTest, RestoreCreatesMissingDestinationParentDirectories) {
    const auto sourcePath = SourcePath(".config/nvim/init.lua");
    cfgsync::tests::WriteTextFile(sourcePath, "vim.opt.number = true\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    fs::remove_all(sourcePath.parent_path().parent_path());
    ASSERT_FALSE(fs::exists(sourcePath.parent_path()));

    const auto result = RunRestoreSingleCommand(sourcePath);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "vim.opt.number = true\n");
}

TEST_F(RestoreCommandCliTest, RestoreOverwritesChangedLocalFile) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "stored contents\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");

    const auto result = RunRestoreSingleCommand(sourcePath);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "stored contents\n");
}

TEST_F(RestoreCommandCliTest, SingleRestoreOfUntrackedFileReturnsNonZero) {
    const auto trackedPath = SourcePath(".gitconfig");
    const auto untrackedPath = SourcePath("untracked.conf");
    cfgsync::tests::WriteTextFile(trackedPath, "[user]\n");
    cfgsync::tests::WriteTextFile(untrackedPath, "untracked\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(trackedPath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);

    const auto result = RunRestoreSingleCommand(untrackedPath);

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("File is not tracked"), std::string::npos);
    EXPECT_NE(result.Error.find(cfgsync::utils::NormalizePath(untrackedPath).string()), std::string::npos);
}

TEST_F(RestoreCommandCliTest, SingleRestoreWithMissingStoredBackupReturnsNonZero) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    fs::remove(cfgsync::tests::StoredPathFor(StorageRoot(), sourcePath));

    const auto result = RunRestoreSingleCommand(sourcePath);

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Path does not exist"), std::string::npos);
    EXPECT_NE(result.Error.find(cfgsync::tests::StoredRegistryPathFor(StorageRoot(), sourcePath).string()),
              std::string::npos);
}

TEST_F(RestoreCommandCliTest, RestoreAllContinuesAfterMissingStoredBackupAndReturnsNonZero) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingBackupPath = SourcePath("missing.conf");
    cfgsync::tests::WriteTextFile(existingPath, "[user]\n");
    cfgsync::tests::WriteTextFile(missingBackupPath, "temporary\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(existingPath));
    ASSERT_TRUE(RunAddCommand(missingBackupPath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(StorageRoot() / "registry.json");
    cfgsync::tests::WriteTextFile(existingPath, "changed contents\n");
    fs::remove(cfgsync::tests::StoredPathFor(StorageRoot(), missingBackupPath));

    const auto result = RunRestoreAllCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("Restored file"), std::string::npos);
    EXPECT_NE(result.Output.find("Failed to restore file"), std::string::npos);
    EXPECT_NE(result.Output.find(cfgsync::utils::NormalizePath(missingBackupPath).string()), std::string::npos);
    EXPECT_NE(result.Error.find("Restore completed with 1 failure."), std::string::npos);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(existingPath), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(StorageRoot() / "registry.json"), registryBeforeRestore);
}

TEST_F(RestoreCommandCliTest, RestoreAllOfEmptyRegistrySucceedsClearly) {
    ASSERT_TRUE(RunInitCommand());

    const auto result = RunRestoreAllCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("No files tracked."), std::string::npos);
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(RestoreCommandCliTest, MissingAppConfigFailsWithInitGuidance) {
    const auto result = RunRestoreAllCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("cfgsync has not been initialized"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync init --storage <dir>"), std::string::npos);
}

TEST_F(RestoreCommandCliTest, MalformedRegistryFailsClearly) {
    ASSERT_TRUE(RunInitCommand());
    cfgsync::tests::WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result = RunRestoreAllCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Malformed cfgsync registry"), std::string::npos);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunCfgsyncCliGoogleTests(argc, argv); }
