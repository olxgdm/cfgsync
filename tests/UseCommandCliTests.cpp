#include "common/CliCommandTestFixture.hpp"
#include "common/CliTestUtils.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/TestFileUtils.hpp"
#include "gtest/gtest.h"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace {
namespace fs = std::filesystem;

class UseCommandCliTest : public cfgsync::tests::CliCommandTestFixture {
protected:
    fs::path MovedStorageRoot() const { return GetTestRoot() / "moved-storage"; }

    fs::path AppConfigPath() const {
#ifdef _WIN32
        return GetTestRoot() / "appdata" / "cfgsync" / "config.json";
#else
        return GetTestRoot() / "home" / ".config" / "cfgsync" / "config.json";
#endif
    }

    cfgsync::tests::CommandResult RunUseCommand(const fs::path& storageRoot) const {
        return RunCommand("use --storage " + cfgsync::tests::QuoteForCommand(storageRoot));
    }

    cfgsync::tests::CommandResult RunListCommand() const { return RunCommand("list"); }

    cfgsync::tests::CommandResult RunBackupCommand() const { return RunCommand("backup"); }

    cfgsync::tests::CommandResult RunRestoreAllCommand() const { return RunCommand("restore --all"); }
};

TEST_F(UseCommandCliTest, UseAdoptsStorageWithoutExistingAppConfigAndListWorks) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    fs::remove(AppConfigPath());
    ASSERT_FALSE(fs::exists(AppConfigPath()));

    const auto useResult = RunUseCommand(StorageRoot());
    const auto listResult = RunListCommand();

    EXPECT_EQ(useResult.ExitCode, 0);
    EXPECT_TRUE(useResult.Error.empty());
    EXPECT_EQ(listResult.ExitCode, 0);
    EXPECT_NE(listResult.Output.find(cfgsync::utils::NormalizePath(sourcePath).string()), std::string::npos);
    EXPECT_TRUE(listResult.Error.empty());
}

TEST_F(UseCommandCliTest, UseRewritesMovedRegistryStorageRootSoLaterCommandsWork) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    fs::remove(AppConfigPath());
    fs::rename(StorageRoot(), MovedStorageRoot());

    const auto useResult = RunUseCommand(MovedStorageRoot());
    const auto listResult = RunListCommand();

    const auto registryDocument = cfgsync::tests::ReadJsonFile(MovedStorageRoot() / "registry.json");
    EXPECT_EQ(useResult.ExitCode, 0);
    EXPECT_TRUE(useResult.Error.empty());
    EXPECT_EQ(registryDocument["storage_root"], cfgsync::utils::NormalizePath(MovedStorageRoot()).string());
    EXPECT_EQ(listResult.ExitCode, 0);
    EXPECT_NE(listResult.Output.find(cfgsync::utils::NormalizePath(sourcePath).string()), std::string::npos);
}

TEST_F(UseCommandCliTest, UseDoesNotRestoreFilesByItself) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "stored contents\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    cfgsync::tests::WriteTextFile(sourcePath, "local contents\n");
    fs::remove(AppConfigPath());

    const auto useResult = RunUseCommand(StorageRoot());

    EXPECT_EQ(useResult.ExitCode, 0);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "local contents\n");
}

TEST_F(UseCommandCliTest, UseThenRestoreAllRestoresStoredFiles) {
    const auto sourcePath = SourcePath(".gitconfig");
    cfgsync::tests::WriteTextFile(sourcePath, "stored contents\n");
    ASSERT_TRUE(RunInitCommand());
    ASSERT_TRUE(RunAddCommand(sourcePath));
    ASSERT_EQ(RunBackupCommand().ExitCode, 0);
    cfgsync::tests::WriteTextFile(sourcePath, "local contents\n");
    fs::remove(AppConfigPath());
    ASSERT_EQ(RunUseCommand(StorageRoot()).ExitCode, 0);

    const auto restoreResult = RunRestoreAllCommand();

    EXPECT_EQ(restoreResult.ExitCode, 0);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "stored contents\n");
}

TEST_F(UseCommandCliTest, MissingStorageOptionReturnsCliValidationFailure) {
    const auto result = RunCommand("use");

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("--storage"), std::string::npos);
}

TEST_F(UseCommandCliTest, MalformedRegistryFailsClearly) {
    cfgsync::tests::WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result = RunUseCommand(StorageRoot());

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Malformed cfgsync registry"), std::string::npos);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunCfgsyncCliGoogleTests(argc, argv); }
