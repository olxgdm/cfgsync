#include "common/TestTempDirectory.hpp"
#include "gtest/gtest.h"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace {
namespace fs = std::filesystem;

fs::path CfgsyncExecutablePath;

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

void SetEnvironmentVariable(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

fs::path ResolveCfgsyncExecutablePath(const char* testExecutablePath) {
    auto executablePath = fs::absolute(fs::path{testExecutablePath}).parent_path() / "cfgsync";
#ifdef _WIN32
    executablePath += ".exe";
#endif
    return executablePath;
}

std::string QuoteForCommand(const fs::path& path) { return "\"" + path.string() + "\""; }

bool CommandSucceeded(const std::string& arguments) {
    auto command = QuoteForCommand(CfgsyncExecutablePath) + " " + arguments;
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    return std::system(command.c_str()) == 0;  // NOSONAR
}

class AddCommandCliTest : public testing::Test {
protected:
    void SetUp() override {
        TestRoot = cfgsync::tests::MakeTestRoot();
#ifdef _WIN32
        SetEnvironmentVariable("APPDATA", (TestRoot / "appdata").string());
#else
        SetEnvironmentVariable("HOME", (TestRoot / "home").string());
#endif
    }

    void TearDown() override { fs::remove_all(TestRoot); }

    fs::path GetTestRoot() const { return TestRoot; }

private:
    fs::path TestRoot;
};

TEST_F(AddCommandCliTest, AddUsesActiveStorageRootPersistedByInit) {
    const auto storageRoot = GetTestRoot() / "storage";
    const auto sourcePath = GetTestRoot() / "configs" / ".gitconfig";
    WriteTextFile(sourcePath, "[user]\n");

    ASSERT_TRUE(CommandSucceeded("init --storage " + QuoteForCommand(storageRoot)));

    ASSERT_TRUE(CommandSucceeded("add " + QuoteForCommand(sourcePath)));

    const auto normalizedSourcePath = cfgsync::utils::NormalizePath(sourcePath);
    const auto document = ReadJsonFile(storageRoot / "registry.json");
    ASSERT_EQ(document["tracked_files"].size(), 1U);
    EXPECT_EQ(document["tracked_files"][0]["original_path"], normalizedSourcePath.string());
    EXPECT_EQ(document["tracked_files"][0]["stored_relative_path"],
              cfgsync::utils::MakeStorageRelativePath(normalizedSourcePath).generic_string());
}

TEST_F(AddCommandCliTest, DuplicateAddReturnsSuccessAndLeavesRegistryUnchanged) {
    const auto storageRoot = GetTestRoot() / "storage";
    const auto sourcePath = GetTestRoot() / "configs" / ".gitconfig";
    WriteTextFile(sourcePath, "[user]\n");

    ASSERT_TRUE(CommandSucceeded("init --storage " + QuoteForCommand(storageRoot)));
    ASSERT_TRUE(CommandSucceeded("add " + QuoteForCommand(sourcePath)));
    const auto documentAfterFirstAdd = ReadJsonFile(storageRoot / "registry.json");

    EXPECT_TRUE(CommandSucceeded("add " + QuoteForCommand(sourcePath)));

    EXPECT_EQ(ReadJsonFile(storageRoot / "registry.json"), documentAfterFirstAdd);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 0) {
        CfgsyncExecutablePath = ResolveCfgsyncExecutablePath(argv[0]);
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
