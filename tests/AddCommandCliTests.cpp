#include "common/CliCommandTestFixture.hpp"
#include "common/CliTestUtils.hpp"
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

}  // namespace

int main(int argc, char** argv) {
    if (argc > 0) {
        cfgsync::tests::InitializeCfgsyncExecutablePath(argv[0]);
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
