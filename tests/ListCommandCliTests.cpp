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

class ListCommandCliTest : public testing::Test {
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

private:
    fs::path TestRoot;
};

TEST_F(ListCommandCliTest, ListAfterInitReportsEmptyRegistry) {
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));

    const auto result =
        cfgsync::tests::RunCfgsyncCommand("list", GetTestRoot() / "list.out", GetTestRoot() / "list.err");

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(result.Output, "No files tracked.\n");
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(ListCommandCliTest, ListPrintsTrackedOriginalPaths) {
    const auto firstPath = GetTestRoot() / "configs" / ".gitconfig";
    const auto secondPath = GetTestRoot() / "configs" / "init.lua";
    WriteTextFile(firstPath, "[user]\n");
    WriteTextFile(secondPath, "vim.opt.number = true\n");
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(firstPath), GetTestRoot()));
    ASSERT_TRUE(
        cfgsync::tests::CfgsyncCommandSucceeded("add " + cfgsync::tests::QuoteForCommand(secondPath), GetTestRoot()));
    const auto registryBeforeList = ReadJsonFile(StorageRoot() / "registry.json");

    const auto result =
        cfgsync::tests::RunCfgsyncCommand("list", GetTestRoot() / "list.out", GetTestRoot() / "list.err");

    const auto normalizedFirstPath = cfgsync::utils::NormalizePath(firstPath);
    const auto normalizedSecondPath = cfgsync::utils::NormalizePath(secondPath);
    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(result.Output, normalizedFirstPath.string() + "\n" + normalizedSecondPath.string() + "\n");
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(ReadJsonFile(StorageRoot() / "registry.json"), registryBeforeList);
}

TEST_F(ListCommandCliTest, MissingAppConfigFailsWithInitGuidance) {
    const auto result =
        cfgsync::tests::RunCfgsyncCommand("list", GetTestRoot() / "list.out", GetTestRoot() / "list.err");

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("cfgsync has not been initialized"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync init --storage <dir>"), std::string::npos);
}

TEST_F(ListCommandCliTest, MalformedRegistryFailsClearly) {
    ASSERT_TRUE(cfgsync::tests::CfgsyncCommandSucceeded(
        "init --storage " + cfgsync::tests::QuoteForCommand(StorageRoot()), GetTestRoot()));
    WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result =
        cfgsync::tests::RunCfgsyncCommand("list", GetTestRoot() / "list.out", GetTestRoot() / "list.err");

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
