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

class StatusCommandCliTest : public cfgsync::tests::CliCommandTestFixture {
protected:
    cfgsync::tests::CommandResult RunBackupCommand() const { return RunCommand("backup"); }

    cfgsync::tests::CommandResult RunStatusCommand() const { return RunCommand("status"); }
};

TEST_F(StatusCommandCliTest, CleanStatusAfterBackup) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);

    const auto result = RunStatusCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(result.Output, "Clean.\n");
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(StatusCommandCliTest, ModifiedStatusAfterSourceEdit) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "stored contents\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");

    const auto result = RunStatusCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(result.Output, "modified " + cfgsync::utils::NormalizePath(sourcePath).string() + "\n");
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(StatusCommandCliTest, MissingOriginalStatusAfterSourceRemoval) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    fs::remove(sourcePath);

    const auto result = RunStatusCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(result.Output, "missing-original " + cfgsync::utils::NormalizePath(sourcePath).string() + "\n");
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(StatusCommandCliTest, MissingBackupStatusWhenTrackedFileHasNoStoredCopy) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));

    const auto result = RunStatusCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(result.Output, "missing-backup " + cfgsync::utils::NormalizePath(sourcePath).string() + "\n");
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(StatusCommandCliTest, MissingAppConfigFailsWithInitGuidance) {
    const auto result = RunStatusCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("cfgsync has not been initialized"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync init --storage <dir>"), std::string::npos);
}

TEST_F(StatusCommandCliTest, MalformedRegistryFailsClearly) {
    ASSERT_TRUE(RunInitCommand());
    cfgsync::tests::WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result = RunStatusCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Malformed cfgsync registry"), std::string::npos);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunCfgsyncCliGoogleTests(argc, argv); }
