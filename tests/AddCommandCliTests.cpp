#include "common/CliCommandTestFixture.hpp"
#include "common/CliTestUtils.hpp"
#include "gtest/gtest.h"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

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

class AddCommandCliTest : public cfgsync::tests::CliCommandTestFixture {};

TEST_F(AddCommandCliTest, AddUsesActiveStorageRootPersistedByInit) {
    const auto sourcePath = SourcePath(".gitconfig");
    WriteTextFile(sourcePath, "[user]\n");

    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));

    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot()));

    const auto normalizedSourcePath = cfgsync::utils::NormalizePath(sourcePath);
    const auto document = ReadJsonFile(StorageRoot() / "registry.json");
    ASSERT_EQ(document["tracked_files"].size(), 1U);
    EXPECT_EQ(document["tracked_files"][0]["original_path"], normalizedSourcePath.string());
    EXPECT_EQ(document["tracked_files"][0]["stored_relative_path"],
              cfgsync::utils::MakeStorageRelativePath(normalizedSourcePath).generic_string());
}

TEST_F(AddCommandCliTest, DuplicateAddReturnsSuccessAndLeavesRegistryUnchanged) {
    const auto sourcePath = SourcePath(".gitconfig");
    WriteTextFile(sourcePath, "[user]\n");

    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot()));
    const auto documentAfterFirstAdd = ReadJsonFile(StorageRoot() / "registry.json");

    EXPECT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(sourcePath), GetTestRoot()));

    EXPECT_EQ(ReadJsonFile(StorageRoot() / "registry.json"), documentAfterFirstAdd);
}

TEST_F(AddCommandCliTest, AddDirectoryUsesActiveStorageRootAndWritesImportedFilesToRegistry) {
    const auto directoryPath = GetTestRoot() / "configs";
    const auto firstPath = directoryPath / "git" / ".gitconfig";
    const auto secondPath = directoryPath / "nvim" / "init.lua";
    WriteTextFile(secondPath, "vim.opt.number = true\n");
    WriteTextFile(firstPath, "[user]\n");

    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));

    const auto result = RunCommand("add " + cfgsync::tests::QuoteForCommand(directoryPath));

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_TRUE(result.Error.empty());

    const std::vector<fs::path> expectedPaths{
        cfgsync::utils::NormalizePath(firstPath),
        cfgsync::utils::NormalizePath(secondPath),
    };
    const auto document = ReadJsonFile(StorageRoot() / "registry.json");
    ASSERT_EQ(document["tracked_files"].size(), expectedPaths.size());
    for (std::size_t index = 0; index < expectedPaths.size(); ++index) {
        EXPECT_EQ(document["tracked_files"][index]["original_path"], expectedPaths[index].string());
        EXPECT_EQ(document["tracked_files"][index]["stored_relative_path"],
                  cfgsync::utils::MakeStorageRelativePath(expectedPaths[index]).generic_string());
    }
}

TEST_F(AddCommandCliTest, RepeatedDirectoryAddSucceedsAndLeavesRegistryUnchanged) {
    const auto directoryPath = GetTestRoot() / "configs";
    const auto firstPath = directoryPath / ".gitconfig";
    const auto secondPath = directoryPath / "nvim" / "init.lua";
    WriteTextFile(firstPath, "[user]\n");
    WriteTextFile(secondPath, "vim.opt.number = true\n");

    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(directoryPath),
                                                        GetTestRoot()));
    const auto documentAfterFirstAdd = ReadJsonFile(StorageRoot() / "registry.json");

    const auto result = RunCommand("add " + cfgsync::tests::QuoteForCommand(directoryPath));

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(ReadJsonFile(StorageRoot() / "registry.json"), documentAfterFirstAdd);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 0) {
        cfgsync::tests::InitializeCfgsyncExecutablePath(argv[0]);
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
