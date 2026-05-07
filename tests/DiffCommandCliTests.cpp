#include "common/CliCommandTestFixture.hpp"
#include "common/CliTestUtils.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/TestFileUtils.hpp"
#include "gtest/gtest.h"
#include "utils/PathUtils.hpp"
#include "utils/TerminalStyle.hpp"

#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;

std::string StyledHeader(const std::string& text) {
    return cfgsync::utils::Colorizer::Enabled().Apply(
        text, cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Cyan).Bold());
}

std::string StyledHunk(const std::string& text) {
    return cfgsync::utils::Colorizer::Enabled().Apply(
        text, cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Yellow));
}

std::string StyledRemoved(const std::string& text) {
    return cfgsync::utils::Colorizer::Enabled().Apply(
        text, cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Red));
}

std::string StyledAdded(const std::string& text) {
    return cfgsync::utils::Colorizer::Enabled().Apply(
        text, cfgsync::utils::TerminalStyle::Foreground(cfgsync::utils::TerminalColor::Green));
}

class DiffCommandCliTest : public cfgsync::tests::CliCommandTestFixture {
protected:
    cfgsync::tests::CommandResult RunBackupCommand() const { return RunCommand("backup"); }

    cfgsync::tests::CommandResult RunDiffCommand(const fs::path& sourcePath) const {
        return RunCommand("diff " + cfgsync::tests::QuoteForCommand(sourcePath));
    }

    std::string DisplayPathFor(const fs::path& sourcePath) const {
        const auto storedRelativePath =
            cfgsync::utils::MakeStorageRelativePath(cfgsync::utils::NormalizePath(sourcePath)).generic_string();
        constexpr std::string_view storagePrefix = "files/";
        if (storedRelativePath.starts_with(storagePrefix)) {
            return storedRelativePath.substr(storagePrefix.size());
        }

        return storedRelativePath;
    }
};

TEST_F(DiffCommandCliTest, ModifiedTrackedFilePrintsUnifiedDiff) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "stored contents\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");

    const auto result = RunDiffCommand(sourcePath);
    const auto displayPath = DisplayPathFor(sourcePath);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(result.Output, StyledHeader("--- stored/" + displayPath) + "\n" +
                                 StyledHeader("+++ original/" + displayPath) + "\n" + StyledHunk("@@ -1,1 +1,1 @@") +
                                 "\n" + StyledRemoved("-stored contents") + "\n" + StyledAdded("+local changes") +
                                 "\n");
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(DiffCommandCliTest, CleanTrackedFilePrintsNothing) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);

    const auto result = RunDiffCommand(sourcePath);

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(DiffCommandCliTest, UntrackedFileFailsClearly) {
    const auto trackedPath = SourcePath(".gitconfig");
    const auto untrackedPath = SourcePath("untracked.conf");
    cfgsync::tests::WriteTextFile(trackedPath, "[user]\n");
    cfgsync::tests::WriteTextFile(untrackedPath, "untracked\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(trackedPath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);

    const auto result = RunDiffCommand(untrackedPath);

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("File is not tracked"), std::string::npos);
    EXPECT_NE(result.Error.find(cfgsync::utils::NormalizePath(untrackedPath).string()), std::string::npos);
}

TEST_F(DiffCommandCliTest, TrackedButNeverBackedUpFailsWithGuidance) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));

    const auto result = RunDiffCommand(sourcePath);

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("no stored backup yet"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync backup"), std::string::npos);
}

TEST_F(DiffCommandCliTest, MissingBackupFailsWithStoredPathContext) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    fs::remove(cfgsync::tests::StoredPathFor(StorageRoot(), sourcePath));

    const auto result = RunDiffCommand(sourcePath);

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("no stored backup yet"), std::string::npos);
    EXPECT_NE(result.Error.find(cfgsync::tests::StoredRegistryPathFor(StorageRoot(), sourcePath).string()),
              std::string::npos);
}

TEST_F(DiffCommandCliTest, MissingOriginalFailsClearly) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    fs::remove(sourcePath);

    const auto result = RunDiffCommand(sourcePath);

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Original file is missing"), std::string::npos);
    EXPECT_NE(result.Error.find(cfgsync::utils::NormalizePath(sourcePath).string()), std::string::npos);
}

TEST_F(DiffCommandCliTest, MissingAppConfigFailsWithInitGuidance) {
    const auto result = RunDiffCommand(SourcePath(".gitconfig"));

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("cfgsync has not been initialized"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync init --storage <dir>"), std::string::npos);
}

TEST_F(DiffCommandCliTest, MalformedRegistryFailsClearly) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    cfgsync::tests::WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result = RunDiffCommand(sourcePath);

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Malformed cfgsync registry"), std::string::npos);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunCfgsyncCliGoogleTests(argc, argv); }
