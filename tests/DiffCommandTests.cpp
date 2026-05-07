#include "Exceptions.hpp"
#include "commands/DiffCommand.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "gtest/gtest.h"
#include "storage/StorageManager.hpp"
#include "utils/PathUtils.hpp"
#include "utils/TerminalStyle.hpp"

#include <filesystem>
#include <sstream>
#include <string>

namespace {
namespace fs = std::filesystem;
using cfgsync::tests::TrackFile;

std::string DisplayPathFor(const cfgsync::core::TrackedEntry& entry) {
    const std::string storedRelativePath = fs::path{entry.StoredRelativePath}.generic_string();
    constexpr std::string_view storagePrefix = "files/";
    if (storedRelativePath.starts_with(storagePrefix)) {
        return storedRelativePath.substr(storagePrefix.size());
    }

    return storedRelativePath;
}

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

class DiffCommandTest : public cfgsync::tests::RegistryCommandTestFixture {
protected:
    std::string RunDiffCommand(const fs::path& sourcePath) {
        cfgsync::storage::StorageManager storageManager{StorageRoot()};
        const cfgsync::commands::DiffCommand command{Registry(), storageManager};
        std::ostringstream output;
        command.Execute(sourcePath, output);
        return output.str();
    }
};

TEST_F(DiffCommandTest, ModifiedTrackedFilePrintsUnifiedDiff) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");
    const auto& entry = Registry().GetTrackedEntries().front();

    EXPECT_EQ(RunDiffCommand(sourcePath), StyledHeader("--- stored/" + DisplayPathFor(entry)) + "\n" +
                                              StyledHeader("+++ original/" + DisplayPathFor(entry)) + "\n" +
                                              StyledHunk("@@ -1,1 +1,1 @@") + "\n" + StyledRemoved("-stored contents") +
                                              "\n" + StyledAdded("+local changes") + "\n");
}

TEST_F(DiffCommandTest, IdenticalTrackedFilePrintsNothing) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");

    EXPECT_TRUE(RunDiffCommand(sourcePath).empty());
}

TEST_F(DiffCommandTest, UntrackedFileFailsClearly) {
    try {
        static_cast<void>(RunDiffCommand(SourcePath()));
        FAIL() << "Diff of an untracked file did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("File is not tracked"), std::string::npos);
        EXPECT_NE(message.find(cfgsync::utils::NormalizePath(SourcePath()).string()), std::string::npos);
    }
}

TEST_F(DiffCommandTest, MissingOriginalFailsClearly) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");

    try {
        static_cast<void>(RunDiffCommand(sourcePath));
        FAIL() << "Diff with a missing original did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Original file is missing"), std::string::npos);
        EXPECT_NE(message.find(Registry().GetTrackedEntries().front().OriginalPath), std::string::npos);
    }
}

TEST_F(DiffCommandTest, MissingBackupFailsClearly) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(sourcePath, "local contents\n");

    try {
        static_cast<void>(RunDiffCommand(sourcePath));
        FAIL() << "Diff with a missing backup did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("no stored backup yet"), std::string::npos);
        EXPECT_NE(message.find((StorageRoot() / storedRelativePath).string()), std::string::npos);
    }
}

TEST_F(DiffCommandTest, TrackedButNeverBackedUpUsesMissingBackupGuidance) {
    const auto sourcePath = SourcePath("never-backed-up.conf");
    cfgsync::tests::WriteTextFile(sourcePath, "local contents\n");
    TrackFile(Registry(), sourcePath);

    try {
        static_cast<void>(RunDiffCommand(sourcePath));
        FAIL() << "Diff of a never-backed-up file did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("no stored backup yet"), std::string::npos);
        EXPECT_NE(message.find("cfgsync backup"), std::string::npos);
    }
}

TEST_F(DiffCommandTest, ExecuteDoesNotMutateOrSaveRegistry) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");
    cfgsync::tests::WriteTextFile(sourcePath, "local contents\n");
    const auto registryBeforeDiff = cfgsync::tests::ReadJsonFile(RegistryPath());

    EXPECT_FALSE(RunDiffCommand(sourcePath).empty());
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeDiff);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
