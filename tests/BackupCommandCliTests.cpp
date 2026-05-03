#include "common/CliCommandTestFixture.hpp"
#include "common/CliTestUtils.hpp"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string>

namespace {
namespace fs = std::filesystem;

void WriteTextFile(const fs::path& path, const std::string& contents) {
    if (path.has_parent_path()) {
        cfgsync::utils::EnsureDirectoryExists(path.parent_path());
    }

    std::ofstream output{path};
    output << contents;
}

std::string ReadTextFile(const fs::path& path) {
    std::ifstream input{path};
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

nlohmann::json ReadJsonFile(const fs::path& path) {
    std::ifstream input{path};
    nlohmann::json document;
    input >> document;
    return document;
}

fs::path StoredPathFor(const fs::path& storageRoot, const fs::path& sourcePath) {
    const auto normalizedSourcePath = cfgsync::utils::NormalizePath(sourcePath);
    return storageRoot / cfgsync::utils::MakeStorageRelativePath(normalizedSourcePath);
}

class BackupCommandCliTest : public cfgsync::tests::CliCommandTestFixture {};

TEST_F(BackupCommandCliTest, BackupUsesActiveStorageRootPersistedByInit) {
    const auto sourcePath = SourcePath(".gitconfig");
    WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot()));

    const auto result =
        cfgsync::tests::RunCfgsyncCommand("backup", GetTestRoot() / "backup.out", GetTestRoot() / "backup.err");

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("Backed up file"), std::string::npos);
    EXPECT_NE(result.Output.find(cfgsync::utils::NormalizePath(sourcePath).string()), std::string::npos);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(ReadTextFile(StoredPathFor(StorageRoot(), sourcePath)), "[user]\n");
}

TEST_F(BackupCommandCliTest, BackupCopiesMultipleTrackedFiles) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("init.lua");
    WriteTextFile(firstPath, "[user]\n");
    WriteTextFile(secondPath, "vim.opt.number = true\n");
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(firstPath), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(secondPath), GetTestRoot()));

    const auto result =
        cfgsync::tests::RunCfgsyncCommand("backup", GetTestRoot() / "backup.out", GetTestRoot() / "backup.err");

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(ReadTextFile(StoredPathFor(StorageRoot(), firstPath)), "[user]\n");
    EXPECT_EQ(ReadTextFile(StoredPathFor(StorageRoot(), secondPath)), "vim.opt.number = true\n");
}

TEST_F(BackupCommandCliTest, BackupOverwritesExistingStoredCopy) {
    const auto sourcePath = SourcePath(".gitconfig");
    WriteTextFile(sourcePath, "new contents\n");
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot()));
    WriteTextFile(StoredPathFor(StorageRoot(), sourcePath), "old contents\n");

    const auto result =
        cfgsync::tests::RunCfgsyncCommand("backup", GetTestRoot() / "backup.out", GetTestRoot() / "backup.err");

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(ReadTextFile(StoredPathFor(StorageRoot(), sourcePath)), "new contents\n");
}

TEST_F(BackupCommandCliTest, BackupContinuesAfterMissingSourceAndReturnsNonZero) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingPath = SourcePath("missing.conf");
    WriteTextFile(existingPath, "[user]\n");
    WriteTextFile(missingPath, "temporary\n");
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(existingPath), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(missingPath), GetTestRoot()));
    const auto registryBeforeBackup = ReadJsonFile(StorageRoot() / "registry.json");
    fs::remove(missingPath);

    const auto result =
        cfgsync::tests::RunCfgsyncCommand("backup", GetTestRoot() / "backup.out", GetTestRoot() / "backup.err");

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("Backed up file"), std::string::npos);
    EXPECT_NE(result.Output.find("Failed to back up file"), std::string::npos);
    EXPECT_NE(result.Output.find(cfgsync::utils::NormalizePath(missingPath).string()), std::string::npos);
    EXPECT_NE(result.Error.find("Backup completed with 1 failure."), std::string::npos);
    EXPECT_EQ(ReadTextFile(StoredPathFor(StorageRoot(), existingPath)), "[user]\n");
    EXPECT_EQ(ReadJsonFile(StorageRoot() / "registry.json"), registryBeforeBackup);
}

TEST_F(BackupCommandCliTest, BackupOfEmptyRegistrySucceedsClearly) {
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));

    const auto result =
        cfgsync::tests::RunCfgsyncCommand("backup", GetTestRoot() / "backup.out", GetTestRoot() / "backup.err");

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("No files tracked."), std::string::npos);
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(BackupCommandCliTest, MissingAppConfigFailsWithInitGuidance) {
    const auto result =
        cfgsync::tests::RunCfgsyncCommand("backup", GetTestRoot() / "backup.out", GetTestRoot() / "backup.err");

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("cfgsync has not been initialized"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync init --storage <dir>"), std::string::npos);
}

TEST_F(BackupCommandCliTest, MalformedRegistryFailsClearly) {
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result =
        cfgsync::tests::RunCfgsyncCommand("backup", GetTestRoot() / "backup.out", GetTestRoot() / "backup.err");

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Malformed cfgsync registry"), std::string::npos);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 0) {
        cfgsync::tests::InitializeCfgsyncExecutablePath(argv[0]);
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
