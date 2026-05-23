#include "Exceptions.hpp"
#include "commands/AddCommand.hpp"
#include "common/RegistryCommandTestFixture.hpp"
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

class AddCommandTest : public cfgsync::tests::RegistryCommandTestFixture {};

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

TEST_F(AddCommandTest, ImportsNestedOrdinaryFilesRecursivelyInDeterministicOrder) {
    const auto directoryPath = SourcePath().parent_path();
    const auto firstPath = directoryPath / "zsh" / ".zshrc";
    const auto secondPath = directoryPath / "git" / ".gitconfig";
    const auto thirdPath = directoryPath / "nvim" / "init.lua";
    WriteTextFile(firstPath, "zsh\n");
    WriteTextFile(secondPath, "git\n");
    WriteTextFile(thirdPath, "nvim\n");
    cfgsync::commands::AddCommand command{Registry()};

    command.Execute(directoryPath);

    const std::vector<fs::path> expectedPaths{
        cfgsync::utils::NormalizePath(secondPath),
        cfgsync::utils::NormalizePath(thirdPath),
        cfgsync::utils::NormalizePath(firstPath),
    };

    ASSERT_EQ(Registry().GetTrackedEntries().size(), expectedPaths.size());
    const auto document = ReadJsonFile(RegistryPath());
    ASSERT_EQ(document["tracked_files"].size(), expectedPaths.size());
    for (std::size_t index = 0; index < expectedPaths.size(); ++index) {
        EXPECT_EQ(Registry().GetTrackedEntries()[index].OriginalPath, expectedPaths[index].string());
        EXPECT_EQ(Registry().GetTrackedEntries()[index].StoredRelativePath,
                  cfgsync::utils::MakeStorageRelativePath(expectedPaths[index]).generic_string());
        EXPECT_EQ(document["tracked_files"][index]["original_path"], expectedPaths[index].string());
        EXPECT_EQ(document["tracked_files"][index]["stored_relative_path"],
                  cfgsync::utils::MakeStorageRelativePath(expectedPaths[index]).generic_string());
    }
}

TEST_F(AddCommandTest, DirectoryImportSkipsAlreadyTrackedFilesWithoutSavingAgain) {
    const auto directoryPath = SourcePath().parent_path();
    const auto firstPath = directoryPath / ".gitconfig";
    const auto secondPath = directoryPath / "nvim" / "init.lua";
    WriteTextFile(firstPath, "git\n");
    WriteTextFile(secondPath, "nvim\n");
    cfgsync::commands::AddCommand command{Registry()};

    command.Execute(directoryPath);
    const auto documentAfterFirstImport = ReadJsonFile(RegistryPath());

    command.Execute(directoryPath);

    EXPECT_EQ(Registry().GetTrackedEntries().size(), 2U);
    EXPECT_EQ(ReadJsonFile(RegistryPath()), documentAfterFirstImport);
}

TEST_F(AddCommandTest, DirectoryImportSkipsSymlinksAndKeepsImportingOrdinaryFiles) {
    const auto directoryPath = SourcePath().parent_path();
    const auto sourcePath = directoryPath / ".gitconfig";
    const auto symlinkPath = directoryPath / "linked.conf";
    WriteTextFile(sourcePath, "[user]\n");

    std::error_code errorCode;
    fs::create_symlink(sourcePath, symlinkPath, errorCode);
    if (errorCode) {
        GTEST_SKIP() << "Symlink creation is not available in this test environment.";
    }

    cfgsync::commands::AddCommand command{Registry()};

    command.Execute(directoryPath);

    ASSERT_EQ(Registry().GetTrackedEntries().size(), 1U);
    EXPECT_EQ(Registry().GetTrackedEntries()[0].OriginalPath, cfgsync::utils::NormalizePath(sourcePath).string());
}

TEST_F(AddCommandTest, EmptyDirectoryImportSucceedsWithoutChangingRegistry) {
    const auto directoryPath = SourcePath().parent_path();
    cfgsync::utils::EnsureDirectoryExists(directoryPath);
    const auto registryBeforeAdd = ReadJsonFile(RegistryPath());
    cfgsync::commands::AddCommand command{Registry()};

    EXPECT_NO_THROW(command.Execute(directoryPath));

    EXPECT_TRUE(Registry().GetTrackedEntries().empty());
    EXPECT_EQ(ReadJsonFile(RegistryPath()), registryBeforeAdd);
}

TEST_F(AddCommandTest, DirectoryImportDoesNotWriteBackupCopies) {
    const auto directoryPath = SourcePath().parent_path();
    const auto sourcePath = directoryPath / ".gitconfig";
    WriteTextFile(sourcePath, "[user]\n");
    cfgsync::commands::AddCommand command{Registry()};

    command.Execute(directoryPath);

    const auto normalizedSourcePath = cfgsync::utils::NormalizePath(sourcePath);
    EXPECT_FALSE(fs::exists(StorageRoot() / cfgsync::utils::MakeStorageRelativePath(normalizedSourcePath)));
}

#ifndef _WIN32
TEST_F(AddCommandTest, DirectoryImportContinuesAfterUnreadableSubdirectoryWhenPermissionsApply) {
    const auto directoryPath = SourcePath().parent_path();
    const auto readablePath = directoryPath / ".gitconfig";
    const auto unreadableDirectory = directoryPath / "private";
    const auto hiddenPath = unreadableDirectory / "secret.conf";
    WriteTextFile(readablePath, "[user]\n");
    WriteTextFile(hiddenPath, "secret\n");

    std::error_code errorCode;
    fs::permissions(unreadableDirectory, fs::perms::none, errorCode);
    if (errorCode) {
        GTEST_SKIP() << "Unable to make a test directory unreadable.";
    }

    cfgsync::commands::AddCommand command{Registry()};
    command.Execute(directoryPath);

    fs::permissions(unreadableDirectory, fs::perms::owner_all, errorCode);

    ASSERT_EQ(Registry().GetTrackedEntries().size(), 1U);
    EXPECT_EQ(Registry().GetTrackedEntries()[0].OriginalPath, cfgsync::utils::NormalizePath(readablePath).string());
}
#endif

TEST_F(AddCommandTest, MissingFileFailsClearly) {
    cfgsync::utils::EnsureDirectoryExists(SourcePath().parent_path());
    cfgsync::commands::AddCommand command{Registry()};

    try {
        command.Execute(SourcePath());
        FAIL() << "Missing file did not throw.";
    } catch (const cfgsync::FileError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Path does not exist"), std::string::npos);
        EXPECT_NE(message.find(SourcePath().filename().string()), std::string::npos);
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
    } catch (const cfgsync::FileError& error) {
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
