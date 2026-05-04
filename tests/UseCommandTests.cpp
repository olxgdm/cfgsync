#include "Exceptions.hpp"
#include "commands/UseCommand.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "common/TestTempDirectory.hpp"
#include "core/AppConfig.hpp"
#include "core/Registry.hpp"
#include "gtest/gtest.h"
#include "storage/StorageManager.hpp"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace {
namespace fs = std::filesystem;

class UseCommandTest : public testing::Test {
protected:
    void SetUp() override { TestRoot_ = cfgsync::tests::MakeTestRoot(); }

    void TearDown() override { fs::remove_all(TestRoot_); }

    fs::path StorageRoot() const { return TestRoot_ / "storage"; }

    fs::path MovedStorageRoot() const { return TestRoot_ / "moved-storage"; }

    fs::path AppConfigPath() const { return TestRoot_ / "config" / "cfgsync" / "config.json"; }

    fs::path SourcePath(const std::string& filename = ".gitconfig") const {
        return TestRoot_ / "home" / "user" / filename;
    }

    void InitializeStorageWithTrackedFile(const fs::path& storageRoot, const fs::path& sourcePath) const {
        cfgsync::core::Registry registry{storageRoot / "registry.json"};
        registry.SetStorageRoot(storageRoot);
        registry.Save();
        cfgsync::tests::TrackFile(registry, sourcePath);
        cfgsync::utils::EnsureDirectoryExists(storageRoot / "files");
    }

    void WriteRegistryDocument(const fs::path& storageRoot, const nlohmann::json& document) const {
        cfgsync::tests::WriteTextFile(storageRoot / "registry.json", document.dump(4) + "\n");
    }

private:
    fs::path TestRoot_;
};

TEST_F(UseCommandTest, AdoptsValidExistingStorageAndSavesAppConfig) {
    const auto sourcePath = SourcePath();
    InitializeStorageWithTrackedFile(StorageRoot(), sourcePath);

    cfgsync::core::Registry registry;
    cfgsync::storage::StorageManager storageManager;
    cfgsync::core::AppConfig appConfig{AppConfigPath()};
    cfgsync::commands::UseCommand command{registry, storageManager, appConfig};

    command.Execute(StorageRoot());

    const auto normalizedStorageRoot = cfgsync::utils::NormalizePath(StorageRoot());
    EXPECT_EQ(storageManager.GetStorageRoot(), normalizedStorageRoot);
    EXPECT_EQ(registry.GetStorageRoot(), normalizedStorageRoot);
    ASSERT_EQ(registry.GetTrackedEntries().size(), 1U);
    EXPECT_EQ(registry.GetTrackedEntries()[0].OriginalPath, cfgsync::utils::NormalizePath(sourcePath).string());

    const auto configDocument = cfgsync::tests::ReadJsonFile(AppConfigPath());
    EXPECT_EQ(configDocument["storage_root"], normalizedStorageRoot.string());
}

TEST_F(UseCommandTest, RewritesMovedRegistryStorageRootAndPreservesTrackedEntries) {
    const auto sourcePath = SourcePath(".config");
    InitializeStorageWithTrackedFile(StorageRoot(), sourcePath);
    fs::rename(StorageRoot(), MovedStorageRoot());

    cfgsync::core::Registry registry;
    cfgsync::storage::StorageManager storageManager;
    cfgsync::core::AppConfig appConfig{AppConfigPath()};
    cfgsync::commands::UseCommand command{registry, storageManager, appConfig};

    command.Execute(MovedStorageRoot());

    const auto normalizedMovedStorageRoot = cfgsync::utils::NormalizePath(MovedStorageRoot());
    const auto registryDocument = cfgsync::tests::ReadJsonFile(MovedStorageRoot() / "registry.json");
    ASSERT_EQ(registryDocument["tracked_files"].size(), 1U);
    EXPECT_EQ(registryDocument["storage_root"], normalizedMovedStorageRoot.string());
    EXPECT_EQ(registryDocument["tracked_files"][0]["original_path"],
              cfgsync::utils::NormalizePath(sourcePath).string());
    EXPECT_EQ(registryDocument["tracked_files"][0]["stored_relative_path"],
              cfgsync::utils::MakeStorageRelativePath(cfgsync::utils::NormalizePath(sourcePath)).generic_string());
}

TEST_F(UseCommandTest, RecreatesMissingFilesDirectory) {
    InitializeStorageWithTrackedFile(StorageRoot(), SourcePath());
    fs::remove_all(StorageRoot() / "files");
    ASSERT_FALSE(fs::exists(StorageRoot() / "files"));

    cfgsync::core::Registry registry;
    cfgsync::storage::StorageManager storageManager;
    cfgsync::core::AppConfig appConfig{AppConfigPath()};
    cfgsync::commands::UseCommand command{registry, storageManager, appConfig};

    command.Execute(StorageRoot());

    EXPECT_TRUE(fs::is_directory(StorageRoot() / "files"));
}

TEST_F(UseCommandTest, MissingStorageDirectoryFailsWithoutSavingAppConfig) {
    cfgsync::core::Registry registry;
    cfgsync::storage::StorageManager storageManager;
    cfgsync::core::AppConfig appConfig{AppConfigPath()};
    cfgsync::commands::UseCommand command{registry, storageManager, appConfig};

    try {
        command.Execute(StorageRoot());
        FAIL() << "Missing storage directory did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("cfgsync storage directory does not exist"), std::string::npos);
    }

    EXPECT_FALSE(fs::exists(AppConfigPath()));
}

TEST_F(UseCommandTest, StoragePathThatIsNotDirectoryFailsWithoutSavingAppConfig) {
    cfgsync::tests::WriteTextFile(StorageRoot(), "not a directory\n");

    cfgsync::core::Registry registry;
    cfgsync::storage::StorageManager storageManager;
    cfgsync::core::AppConfig appConfig{AppConfigPath()};
    cfgsync::commands::UseCommand command{registry, storageManager, appConfig};

    try {
        command.Execute(StorageRoot());
        FAIL() << "Storage path that is not a directory did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("cfgsync storage path is not a directory"), std::string::npos);
    }

    EXPECT_FALSE(fs::exists(AppConfigPath()));
}

TEST_F(UseCommandTest, MissingRegistryFailsWithoutSavingAppConfig) {
    cfgsync::utils::EnsureDirectoryExists(StorageRoot());

    cfgsync::core::Registry registry;
    cfgsync::storage::StorageManager storageManager;
    cfgsync::core::AppConfig appConfig{AppConfigPath()};
    cfgsync::commands::UseCommand command{registry, storageManager, appConfig};

    try {
        command.Execute(StorageRoot());
        FAIL() << "Missing registry did not throw.";
    } catch (const cfgsync::RegistryError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("does not contain a registry"), std::string::npos);
    }

    EXPECT_FALSE(fs::exists(AppConfigPath()));
}

TEST_F(UseCommandTest, MalformedRegistryFailsWithoutSavingAppConfig) {
    cfgsync::tests::WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    cfgsync::core::Registry registry;
    cfgsync::storage::StorageManager storageManager;
    cfgsync::core::AppConfig appConfig{AppConfigPath()};
    cfgsync::commands::UseCommand command{registry, storageManager, appConfig};

    try {
        command.Execute(StorageRoot());
        FAIL() << "Malformed registry did not throw.";
    } catch (const cfgsync::RegistryError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Malformed cfgsync registry"), std::string::npos);
    }

    EXPECT_FALSE(fs::exists(AppConfigPath()));
}

TEST_F(UseCommandTest, UnsupportedRegistryFailsWithoutSavingAppConfig) {
    const nlohmann::json registryDocument = {
        {"version", 999},
        {"storage_root", cfgsync::utils::NormalizePath(StorageRoot()).string()},
        {"tracked_files", nlohmann::json::array()},
    };
    WriteRegistryDocument(StorageRoot(), registryDocument);

    cfgsync::core::Registry registry;
    cfgsync::storage::StorageManager storageManager;
    cfgsync::core::AppConfig appConfig{AppConfigPath()};
    cfgsync::commands::UseCommand command{registry, storageManager, appConfig};

    try {
        command.Execute(StorageRoot());
        FAIL() << "Unsupported registry version did not throw.";
    } catch (const cfgsync::RegistryError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Unsupported cfgsync registry version"), std::string::npos);
    }

    EXPECT_FALSE(fs::exists(AppConfigPath()));
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
