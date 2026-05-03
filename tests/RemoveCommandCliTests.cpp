#include "common/CliTestUtils.hpp"
#include "common/TestTempDirectory.hpp"
#include "gtest/gtest.h"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace {
namespace fs = std::filesystem;

nlohmann::json ReadJsonFile(const fs::path& path) {
    std::ifstream input{path};
    nlohmann::json document;
    input >> document;
    return document;
}

void WriteTextFile(const fs::path& path, const std::string& contents) {
    if (path.has_parent_path()) {
        cfgsync::utils::EnsureDirectoryExists(path.parent_path());
    }

    std::ofstream output{path};
    output << contents;
}

class RemoveCommandCliTest : public testing::Test {
protected:
    void SetUp() override {
        TestRoot = cfgsync::tests::MakeTestRoot();
#ifdef _WIN32
        cfgsync::tests::SetEnvironmentVariable("APPDATA", (TestRoot / "appdata").string());
#else
        cfgsync::tests::SetEnvironmentVariable("HOME", (TestRoot / "home").string());
#endif
    }

    void TearDown() override { fs::remove_all(TestRoot); }

    fs::path GetTestRoot() const { return TestRoot; }

    fs::path StorageRoot() const { return TestRoot / "storage"; }

    fs::path SourcePath(const std::string& filename) const { return TestRoot / "configs" / filename; }

private:
    fs::path TestRoot;
};

TEST_F(RemoveCommandCliTest, RemoveUsesActiveStorageRootPersistedByInit) {
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

    const auto result = cfgsync::tests::RunCfgsyncCommand(
        "remove " + cfgsync::tests::QuoteForCommand(firstPath), GetTestRoot() / "remove.out",
        GetTestRoot() / "remove.err");

    const auto normalizedFirstPath = cfgsync::utils::NormalizePath(firstPath);
    const auto normalizedSecondPath = cfgsync::utils::NormalizePath(secondPath);
    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("Removed file from tracking"), std::string::npos);
    EXPECT_NE(result.Output.find(normalizedFirstPath.string()), std::string::npos);
    EXPECT_TRUE(result.Error.empty());

    const auto document = ReadJsonFile(StorageRoot() / "registry.json");
    ASSERT_EQ(document["tracked_files"].size(), 1U);
    EXPECT_EQ(document["tracked_files"][0]["original_path"], normalizedSecondPath.string());
}

TEST_F(RemoveCommandCliTest, RemoveWorksWhenOriginalFileNoLongerExists) {
    const auto sourcePath = SourcePath(".gitconfig");
    WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot()));
    fs::remove(sourcePath);

    const auto result = cfgsync::tests::RunCfgsyncCommand(
        "remove " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot() / "remove.out",
        GetTestRoot() / "remove.err");

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("Removed file from tracking"), std::string::npos);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_TRUE(ReadJsonFile(StorageRoot() / "registry.json")["tracked_files"].empty());
}

TEST_F(RemoveCommandCliTest, RemoveDoesNotDeleteStoredBackupFile) {
    const auto sourcePath = SourcePath(".gitconfig");
    WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot()));

    const auto normalizedSourcePath = cfgsync::utils::NormalizePath(sourcePath);
    const auto storedPath = StorageRoot() / cfgsync::utils::MakeStorageRelativePath(normalizedSourcePath);
    WriteTextFile(storedPath, "[user]\n");

    const auto result = cfgsync::tests::RunCfgsyncCommand(
        "remove " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot() / "remove.out",
        GetTestRoot() / "remove.err");

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_TRUE(fs::exists(storedPath));
    EXPECT_TRUE(ReadJsonFile(StorageRoot() / "registry.json")["tracked_files"].empty());
}

TEST_F(RemoveCommandCliTest, MissingTrackedEntryFailsClearlyAndLeavesRegistryUnchanged) {
    const auto sourcePath = SourcePath(".gitconfig");
    WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot()));
    const auto registryBeforeRemove = ReadJsonFile(StorageRoot() / "registry.json");

    const auto result = cfgsync::tests::RunCfgsyncCommand(
        "remove " + cfgsync::tests::QuoteForCommand(SourcePath("missing.conf")), GetTestRoot() / "remove.out",
        GetTestRoot() / "remove.err");

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("File is not tracked"), std::string::npos);
    EXPECT_NE(result.Error.find("missing.conf"), std::string::npos);
    EXPECT_EQ(ReadJsonFile(StorageRoot() / "registry.json"), registryBeforeRemove);
}

TEST_F(RemoveCommandCliTest, MissingAppConfigFailsWithInitGuidance) {
    const auto result = cfgsync::tests::RunCfgsyncCommand(
        "remove " + cfgsync::tests::QuoteForCommand(SourcePath(".gitconfig")), GetTestRoot() / "remove.out",
        GetTestRoot() / "remove.err");

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("cfgsync has not been initialized"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync init --storage <dir>"), std::string::npos);
}

TEST_F(RemoveCommandCliTest, MalformedRegistryFailsClearly) {
    const auto sourcePath = SourcePath(".gitconfig");
    WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result = cfgsync::tests::RunCfgsyncCommand(
        "remove " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot() / "remove.out",
        GetTestRoot() / "remove.err");

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
