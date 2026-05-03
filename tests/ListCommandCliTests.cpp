#include "common/TestTempDirectory.hpp"
#include "gtest/gtest.h"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string>

namespace {
namespace fs = std::filesystem;

fs::path CfgsyncExecutablePath;

struct CommandResult {
    int ExitCode;
    std::string Output;
    std::string Error;
};

nlohmann::json ReadJsonFile(const fs::path& path) {
    std::ifstream input{path};
    nlohmann::json document;
    input >> document;
    return document;
}

std::string ReadTextFile(const fs::path& path) {
    std::ifstream input{path};
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
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

CommandResult RunCommand(const std::string& arguments, const fs::path& outputPath, const fs::path& errorPath) {
    auto command = QuoteForCommand(CfgsyncExecutablePath) + " " + arguments + " > " + QuoteForCommand(outputPath) +
                   " 2> " + QuoteForCommand(errorPath);
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif

    const auto exitCode = std::system(command.c_str());  // NOSONAR
    return CommandResult{
        .ExitCode = exitCode,
        .Output = ReadTextFile(outputPath),
        .Error = ReadTextFile(errorPath),
    };
}

bool CommandSucceeded(const std::string& arguments, const fs::path& testRoot) {
    const auto result = RunCommand(arguments, testRoot / "command.out", testRoot / "command.err");
    return result.ExitCode == 0;
}

class ListCommandCliTest : public testing::Test {
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

    fs::path StorageRoot() const { return TestRoot / "storage"; }

private:
    fs::path TestRoot;
};

TEST_F(ListCommandCliTest, ListAfterInitReportsEmptyRegistry) {
    ASSERT_TRUE(CommandSucceeded("init --storage " + QuoteForCommand(StorageRoot()), GetTestRoot()));

    const auto result = RunCommand("list", GetTestRoot() / "list.out", GetTestRoot() / "list.err");

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(result.Output, "No files tracked.\n");
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(ListCommandCliTest, ListPrintsTrackedOriginalPaths) {
    const auto firstPath = GetTestRoot() / "configs" / ".gitconfig";
    const auto secondPath = GetTestRoot() / "configs" / "init.lua";
    WriteTextFile(firstPath, "[user]\n");
    WriteTextFile(secondPath, "vim.opt.number = true\n");
    ASSERT_TRUE(CommandSucceeded("init --storage " + QuoteForCommand(StorageRoot()), GetTestRoot()));
    ASSERT_TRUE(CommandSucceeded("add " + QuoteForCommand(firstPath), GetTestRoot()));
    ASSERT_TRUE(CommandSucceeded("add " + QuoteForCommand(secondPath), GetTestRoot()));
    const auto registryBeforeList = ReadJsonFile(StorageRoot() / "registry.json");

    const auto result = RunCommand("list", GetTestRoot() / "list.out", GetTestRoot() / "list.err");

    const auto normalizedFirstPath = cfgsync::utils::NormalizePath(firstPath);
    const auto normalizedSecondPath = cfgsync::utils::NormalizePath(secondPath);
    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_EQ(result.Output, normalizedFirstPath.string() + "\n" + normalizedSecondPath.string() + "\n");
    EXPECT_TRUE(result.Error.empty());
    EXPECT_EQ(ReadJsonFile(StorageRoot() / "registry.json"), registryBeforeList);
}

TEST_F(ListCommandCliTest, MissingAppConfigFailsWithInitGuidance) {
    const auto result = RunCommand("list", GetTestRoot() / "list.out", GetTestRoot() / "list.err");

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("cfgsync has not been initialized"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync init --storage <dir>"), std::string::npos);
}

TEST_F(ListCommandCliTest, MalformedRegistryFailsClearly) {
    ASSERT_TRUE(CommandSucceeded("init --storage " + QuoteForCommand(StorageRoot()), GetTestRoot()));
    WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result = RunCommand("list", GetTestRoot() / "list.out", GetTestRoot() / "list.err");

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Malformed cfgsync registry"), std::string::npos);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 0) {
        CfgsyncExecutablePath = ResolveCfgsyncExecutablePath(argv[0]);
    }

    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
