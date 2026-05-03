#include "commands/AddCommand.hpp"
#include "common/TestTempDirectory.hpp"
#include "core/Registry.hpp"
#include "gtest/gtest.h"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
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

class AddCommandTest : public testing::Test {
protected:
    void SetUp() override {
        TestRoot = cfgsync::tests::MakeTestRoot();
        Registry_.SetStorageRoot(StorageRoot());
        Registry_.SetRegistryPath(RegistryPath());
        Registry_.Save();
    }

    void TearDown() override { fs::remove_all(TestRoot); }

    fs::path SourcePath() const { return TestRoot / "home" / "user" / ".gitconfig"; }

    fs::path StorageRoot() const { return TestRoot / "storage"; }

    fs::path RegistryPath() const { return StorageRoot() / "registry.json"; }

    cfgsync::core::Registry& Registry() { return Registry_; }

private:
    fs::path TestRoot;
    cfgsync::core::Registry Registry_;
};

TEST_F(AddCommandTest, AddsExistingOrdinaryFileAndSavesRegistry) {
    const auto sourcePath = SourcePath();
    WriteTextFile(sourcePath, "[user]\n");
    cfgsync::commands::AddCommand command{Registry()};

    command.Execute(sourcePath);

    const auto normalizedSourcePath = cfgsync::utils::NormalizePath(sourcePath);
    ASSERT_EQ(Registry().GetTrackedEntries().size(), 1U);
    EXPECT_EQ(Registry().GetTrackedEntries()[0].OriginalPath, normalizedSourcePath.string());
    EXPECT_EQ(Registry().GetTrackedEntries()[0].StoredRelativePath,
              cfgsync::utils::MakeStorageRelativePath(normalizedSourcePath).generic_string());

    const auto document = ReadJsonFile(RegistryPath());
    ASSERT_EQ(document["tracked_files"].size(), 1U);
    EXPECT_EQ(document["tracked_files"][0]["original_path"], normalizedSourcePath.string());
    EXPECT_EQ(document["tracked_files"][0]["stored_relative_path"],
              cfgsync::utils::MakeStorageRelativePath(normalizedSourcePath).generic_string());
}

TEST_F(AddCommandTest, DuplicateAddDoesNotCreateAnotherRegistryEntry) {
    const auto sourcePath = SourcePath();
    WriteTextFile(sourcePath, "[user]\n");
    cfgsync::commands::AddCommand command{Registry()};

    command.Execute(sourcePath);
    const auto documentAfterFirstAdd = ReadJsonFile(RegistryPath());

    EXPECT_NO_THROW(command.Execute(sourcePath / ".." / ".gitconfig"));

    EXPECT_EQ(Registry().GetTrackedEntries().size(), 1U);
    EXPECT_EQ(ReadJsonFile(RegistryPath()), documentAfterFirstAdd);
}

TEST_F(AddCommandTest, MissingFileFailsClearly) {
    cfgsync::utils::EnsureDirectoryExists(SourcePath().parent_path());
    cfgsync::commands::AddCommand command{Registry()};

    try {
        command.Execute(SourcePath());
        FAIL() << "Missing file did not throw.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Path does not exist"), std::string::npos);
        EXPECT_NE(message.find(SourcePath().filename().string()), std::string::npos);
    }

    EXPECT_TRUE(Registry().GetTrackedEntries().empty());
}

TEST_F(AddCommandTest, DirectoryFailsClearly) {
    const auto directoryPath = SourcePath().parent_path();
    cfgsync::utils::EnsureDirectoryExists(directoryPath);
    cfgsync::commands::AddCommand command{Registry()};

    try {
        command.Execute(directoryPath);
        FAIL() << "Directory path did not throw.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Path is not an ordinary file"), std::string::npos);
        EXPECT_NE(message.find(directoryPath.filename().string()), std::string::npos);
    }

    EXPECT_TRUE(Registry().GetTrackedEntries().empty());
}

TEST_F(AddCommandTest, SymlinkFailsClearly) {
    const auto sourcePath = SourcePath();
    const auto symlinkPath = sourcePath.parent_path() / "linked.conf";
    WriteTextFile(sourcePath, "[user]\n");

    std::error_code errorCode;
    fs::create_symlink(sourcePath, symlinkPath, errorCode);
    if (errorCode) {
        GTEST_SKIP() << "Symlink creation is not available in this test environment.";
    }

    cfgsync::commands::AddCommand command{Registry()};

    try {
        command.Execute(symlinkPath);
        FAIL() << "Symlink path did not throw.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Path is not an ordinary file"), std::string::npos);
        EXPECT_NE(message.find(symlinkPath.filename().string()), std::string::npos);
    }

    EXPECT_TRUE(Registry().GetTrackedEntries().empty());
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
