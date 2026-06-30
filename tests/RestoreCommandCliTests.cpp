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

    cfgsync::tests::CommandResult RunRestoreAllDryRunCommand() const { return RunCommand("restore --all --dry-run"); }

    cfgsync::tests::CommandResult RunRestoreSingleCommand(const fs::path& sourcePath) const {
        return RunCommand("restore " + cfgsync::tests::QuoteForCommand(sourcePath));
    }

    cfgsync::tests::CommandResult RunRestoreSingleDryRunCommand(const fs::path& sourcePath) const {
        return RunCommand("restore " + cfgsync::tests::QuoteForCommand(sourcePath) + " --dry-run");
    }

    cfgsync::tests::CommandResult RunRestoreAllWithRemapCommand(const fs::path& fromPrefix,
                                                                const fs::path& toPrefix) const {
        return RunCommand("restore --all --from-prefix " + cfgsync::tests::QuoteForCommand(fromPrefix) +
                          " --to-prefix " + cfgsync::tests::QuoteForCommand(toPrefix));
    }

    cfgsync::tests::CommandResult RunRestoreSingleWithRemapCommand(const fs::path& sourcePath,
                                                                   const fs::path& fromPrefix,
                                                                   const fs::path& toPrefix) const {
        return RunCommand("restore " + cfgsync::tests::QuoteForCommand(sourcePath) + " --from-prefix " +
                          cfgsync::tests::QuoteForCommand(fromPrefix) + " --to-prefix " +
                          cfgsync::tests::QuoteForCommand(toPrefix));
    }

    cfgsync::tests::CommandResult RunRestoreSingleDryRunWithRemapCommand(const fs::path& sourcePath,
                                                                         const fs::path& fromPrefix,
                                                                         const fs::path& toPrefix) const {
        return RunCommand("restore " + cfgsync::tests::QuoteForCommand(sourcePath) +
                          " --dry-run --from-prefix " + cfgsync::tests::QuoteForCommand(fromPrefix) +
                          " --to-prefix " + cfgsync::tests::QuoteForCommand(toPrefix));
    }

    cfgsync::tests::CommandResult RunRestoreAllDryRunWithRemapCommand(const fs::path& fromPrefix,
                                                                      const fs::path& toPrefix) const {
        return RunCommand("restore --all --dry-run --from-prefix " + cfgsync::tests::QuoteForCommand(fromPrefix) +
                          " --to-prefix " + cfgsync::tests::QuoteForCommand(toPrefix));
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

TEST_F(RestoreCommandCliTest, RestoreSingleDryRunDoesNotOverwriteChangedLocalFile) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "stored contents\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");

    const auto result = RunRestoreSingleDryRunCommand(sourcePath);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("would-overwrite"), std::string::npos);
    EXPECT_NE(result.Output.find(cfgsync::utils::NormalizePath(sourcePath).string()), std::string::npos);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "local changes\n");
}

TEST_F(RestoreCommandCliTest, RestoreSingleDryRunReportsOverwriteWhenFileSizesDiffer) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "stored contents are longer\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    cfgsync::tests::WriteTextFile(sourcePath, "local\n");

    const auto result = RunRestoreSingleDryRunCommand(sourcePath);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("would-overwrite " + cfgsync::utils::NormalizePath(sourcePath).string()),
              std::string::npos);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "local\n");
}

TEST_F(RestoreCommandCliTest, RestoreSingleDryRunReportsUnchangedWhenBothFilesAreEmpty) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);

    const auto result = RunRestoreSingleDryRunCommand(sourcePath);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("unchanged " + cfgsync::utils::NormalizePath(sourcePath).string()),
              std::string::npos);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "");
}

TEST_F(RestoreCommandCliTest, RestoreAllDryRunReportsPlannedImpactWithoutMutatingDestinations) {
    const auto createPath = SourcePath("missing.conf");
    const auto overwritePath = SourcePath(".gitconfig");
    const auto unchangedPath = SourcePath("init.lua");
    cfgsync::tests::WriteTextFile(createPath, "created contents\n");
    cfgsync::tests::WriteTextFile(overwritePath, "stored contents\n");
    cfgsync::tests::WriteTextFile(unchangedPath, "same contents\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(createPath));
    ASSERT_TRUE(RunAddCommand(overwritePath));
    ASSERT_TRUE(RunAddCommand(unchangedPath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    fs::remove(createPath);
    cfgsync::tests::WriteTextFile(overwritePath, "local changes\n");

    const auto result = RunRestoreAllDryRunCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("would-create " + cfgsync::utils::NormalizePath(createPath).string()),
              std::string::npos);
    EXPECT_NE(result.Output.find("would-overwrite " + cfgsync::utils::NormalizePath(overwritePath).string()),
              std::string::npos);
    EXPECT_NE(result.Output.find("unchanged " + cfgsync::utils::NormalizePath(unchangedPath).string()),
              std::string::npos);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_FALSE(fs::exists(createPath));
    EXPECT_EQ(cfgsync::tests::ReadTextFile(overwritePath), "local changes\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(unchangedPath), "same contents\n");
}

TEST_F(RestoreCommandCliTest, RestoreSingleWithPrefixRemapRestoresToRemappedDestination) {
    const auto sourcePath = SourcePath(".config/nvim/init.lua");
    const auto fromPrefix = SourcePath(".placeholder").parent_path();
    const auto toPrefix = GetTestRoot() / "new-configs";
    const auto destinationPath = toPrefix / ".config" / "nvim" / "init.lua";
    cfgsync::tests::WriteTextFile(sourcePath, "vim.opt.number = true\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);

    const auto result = RunRestoreSingleWithRemapCommand(sourcePath, fromPrefix, toPrefix);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(cfgsync::tests::ReadTextFile(destinationPath), "vim.opt.number = true\n");
}

TEST_F(RestoreCommandCliTest, RestoreAllWithPrefixRemapRestoresToRemappedDestinations) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath(".config/nvim/init.lua");
    const auto fromPrefix = SourcePath(".placeholder").parent_path();
    const auto toPrefix = GetTestRoot() / "new-configs";
    cfgsync::tests::WriteTextFile(firstPath, "[user]\n");
    cfgsync::tests::WriteTextFile(secondPath, "vim.opt.number = true\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(firstPath));
    ASSERT_TRUE(RunAddCommand(secondPath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);

    const auto result = RunRestoreAllWithRemapCommand(fromPrefix, toPrefix);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(cfgsync::tests::ReadTextFile(toPrefix / ".gitconfig"), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(toPrefix / ".config" / "nvim" / "init.lua"), "vim.opt.number = true\n");
}

TEST_F(RestoreCommandCliTest, RestoreAllDryRunWithPrefixRemapReportsRemappedDestinationsWithoutCreatingFiles) {
    const auto sourcePath = SourcePath(".gitconfig");
    const auto fromPrefix = SourcePath(".placeholder").parent_path();
    const auto toPrefix = GetTestRoot() / "new-configs";
    const auto destinationPath = toPrefix / ".gitconfig";
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);

    const auto result = RunRestoreAllDryRunWithRemapCommand(fromPrefix, toPrefix);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("would-create " + cfgsync::utils::NormalizePath(destinationPath).string()),
              std::string::npos);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_FALSE(fs::exists(destinationPath));
    EXPECT_FALSE(fs::exists(toPrefix));
}

TEST_F(RestoreCommandCliTest, RestoreSingleDryRunWithPrefixRemapOutsidePrefixReturnsNonZero) {
    const auto sourcePath = SourcePath(".gitconfig");
    const auto fromPrefix = GetTestRoot() / "other-configs";
    const auto toPrefix = GetTestRoot() / "new-configs";
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);

    const auto result = RunRestoreSingleDryRunWithRemapCommand(sourcePath, fromPrefix, toPrefix);

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("outside --from-prefix"), std::string::npos);
    EXPECT_NE(result.Error.find(cfgsync::utils::NormalizePath(sourcePath).string()), std::string::npos);
    EXPECT_FALSE(fs::exists(toPrefix / ".gitconfig"));
}

TEST_F(RestoreCommandCliTest, RestoreWithOnlyOnePrefixFlagReturnsNonZero) {
    ASSERT_TRUE(RunInitCommand());

    const auto result = RunCommand("restore --all --from-prefix " +
                                   cfgsync::tests::QuoteForCommand(SourcePath(".placeholder").parent_path()));

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Specify '--from-prefix' and '--to-prefix' together."), std::string::npos);
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
    EXPECT_NE(result.Error.find(cfgsync::tests::StoredPathFor(StorageRoot(), sourcePath).string()), std::string::npos);
}

TEST_F(RestoreCommandCliTest, SingleDryRunWithMissingStoredBackupReturnsNonZero) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    fs::remove(cfgsync::tests::StoredPathFor(StorageRoot(), sourcePath));

    const auto result = RunRestoreSingleDryRunCommand(sourcePath);

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Path does not exist"), std::string::npos);
    EXPECT_NE(result.Error.find(cfgsync::tests::StoredPathFor(StorageRoot(), sourcePath).string()), std::string::npos);
    EXPECT_TRUE(fs::exists(sourcePath));
}

TEST_F(RestoreCommandCliTest, RestoreAllContinuesAfterMissingStoredBackupAndReturnsNonZero) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingBackupPath = SourcePath("missing.conf");
    cfgsync::tests::WriteTextFile(existingPath, "[user]\n");
    cfgsync::tests::WriteTextFile(missingBackupPath, "temporary\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(missingBackupPath));
    ASSERT_TRUE(RunAddCommand(existingPath));
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

TEST_F(RestoreCommandCliTest, RestoreAllDryRunContinuesAfterMissingStoredBackupAndReturnsNonZero) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingBackupPath = SourcePath("missing.conf");
    cfgsync::tests::WriteTextFile(existingPath, "[user]\n");
    cfgsync::tests::WriteTextFile(missingBackupPath, "temporary\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(missingBackupPath));
    ASSERT_TRUE(RunAddCommand(existingPath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(StorageRoot() / "registry.json");
    cfgsync::tests::WriteTextFile(existingPath, "changed contents\n");
    fs::remove(cfgsync::tests::StoredPathFor(StorageRoot(), missingBackupPath));

    const auto result = RunRestoreAllDryRunCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("would-overwrite " + cfgsync::utils::NormalizePath(existingPath).string()),
              std::string::npos);
    EXPECT_NE(result.Output.find("Failed to restore file"), std::string::npos);
    EXPECT_NE(result.Output.find(cfgsync::utils::NormalizePath(missingBackupPath).string()), std::string::npos);
    EXPECT_NE(result.Error.find("Restore completed with 1 failure."), std::string::npos);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(existingPath), "changed contents\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(StorageRoot() / "registry.json"), registryBeforeRestore);
}

TEST_F(RestoreCommandCliTest, RestoreAllDryRunContinuesAfterStoredBackupIsDirectoryAndReturnsNonZero) {
    const auto directoryBackupPath = SourcePath(".gitconfig");
    const auto previewPath = SourcePath("init.lua");
    cfgsync::tests::WriteTextFile(directoryBackupPath, "[user]\n");
    cfgsync::tests::WriteTextFile(previewPath, "vim.opt.number = true\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(directoryBackupPath));
    ASSERT_TRUE(RunAddCommand(previewPath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(StorageRoot() / "registry.json");
    fs::remove(cfgsync::tests::StoredPathFor(StorageRoot(), directoryBackupPath));
    fs::create_directories(cfgsync::tests::StoredPathFor(StorageRoot(), directoryBackupPath));
    fs::remove(previewPath);

    const auto result = RunRestoreAllDryRunCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("Failed to restore file"), std::string::npos);
    EXPECT_NE(result.Output.find("Path is not an ordinary file"), std::string::npos);
    EXPECT_NE(result.Output.find("would-create " + cfgsync::utils::NormalizePath(previewPath).string()),
              std::string::npos);
    EXPECT_NE(result.Error.find("Restore completed with 1 failure."), std::string::npos);
    EXPECT_FALSE(fs::exists(previewPath));
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

TEST_F(RestoreCommandCliTest, RestoreHelpIncludesDryRunFlag) {
    const auto result = RunCommand("restore --help");

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("--dry-run"), std::string::npos);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunCfgsyncCliGoogleTests(argc, argv); }
