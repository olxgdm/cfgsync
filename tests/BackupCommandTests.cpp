#include "commands/BackupCommand.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "storage/StorageManager.hpp"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;

void WriteTextFile(const fs::path& path, const std::string& contents) {
    if (path.has_parent_path()) {
        cfgsync::utils::EnsureDirectoryExists(path.parent_path());
    }

    std::ofstream output{path};
    output << contents;
}

std::string ReadTextFile(const fs::path& path) {
    std::ifstream input{path};
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

nlohmann::json ReadJsonFile(const fs::path& path) {
    std::ifstream input{path};
    nlohmann::json document;
    input >> document;
    return document;
}

fs::path TrackFile(cfgsync::core::Registry& registry, const fs::path& sourcePath) {
    const auto normalizedSourcePath = cfgsync::utils::NormalizePath(sourcePath);
    const auto storedRelativePath = cfgsync::utils::MakeStorageRelativePath(normalizedSourcePath);
    const auto added = registry.AddEntry({
        .OriginalPath = normalizedSourcePath.string(),
        .StoredRelativePath = storedRelativePath.generic_string(),
    });
    EXPECT_TRUE(added);
    registry.Save();
    return storedRelativePath;
}

class BackupCommandTest : public cfgsync::tests::RegistryCommandTestFixture {};

TEST_F(BackupCommandTest, BacksUpOneTrackedFile) {
    const auto sourcePath = SourcePath();
    WriteTextFile(sourcePath, "[user]\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_EQ(ReadTextFile(StorageRoot() / storedRelativePath), "[user]\n");
}

TEST_F(BackupCommandTest, BacksUpMultipleTrackedFiles) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("init.lua");
    WriteTextFile(firstPath, "[user]\n");
    WriteTextFile(secondPath, "vim.opt.number = true\n");
    const auto firstStoredRelativePath = TrackFile(Registry(), firstPath);
    const auto secondStoredRelativePath = TrackFile(Registry(), secondPath);

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_EQ(ReadTextFile(StorageRoot() / firstStoredRelativePath), "[user]\n");
    EXPECT_EQ(ReadTextFile(StorageRoot() / secondStoredRelativePath), "vim.opt.number = true\n");
}

TEST_F(BackupCommandTest, CreatesDestinationParentDirectories) {
    const auto sourcePath = SourcePath(".config/nvim/init.lua");
    WriteTextFile(sourcePath, "vim.opt.number = true\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    ASSERT_FALSE(fs::exists((StorageRoot() / storedRelativePath).parent_path()));

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_TRUE(fs::exists(StorageRoot() / storedRelativePath));
    EXPECT_EQ(ReadTextFile(StorageRoot() / storedRelativePath), "vim.opt.number = true\n");
}

TEST_F(BackupCommandTest, OverwritesExistingStoredCopy) {
    const auto sourcePath = SourcePath();
    WriteTextFile(sourcePath, "new contents\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    WriteTextFile(StorageRoot() / storedRelativePath, "old contents\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    command.Execute();

    EXPECT_EQ(ReadTextFile(StorageRoot() / storedRelativePath), "new contents\n");
}

TEST_F(BackupCommandTest, ContinuesAfterMissingSourceAndReportsPartialFailure) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingPath = SourcePath("missing.conf");
    WriteTextFile(existingPath, "[user]\n");
    const auto existingStoredRelativePath = TrackFile(Registry(), existingPath);
    TrackFile(Registry(), missingPath);
    const auto registryBeforeBackup = ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    try {
        command.Execute();
        FAIL() << "Backup with a missing source did not throw.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Backup completed with 1 failure."), std::string::npos);
    }

    EXPECT_EQ(ReadTextFile(StorageRoot() / existingStoredRelativePath), "[user]\n");
    EXPECT_EQ(ReadJsonFile(RegistryPath()), registryBeforeBackup);
}

TEST_F(BackupCommandTest, EmptyRegistrySucceedsWithoutCreatingStoredFiles) {
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::BackupCommand command{Registry(), storageManager};

    EXPECT_NO_THROW(command.Execute());
    EXPECT_FALSE(fs::exists(StorageRoot() / "files"));
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
