#include "commands/RemoveCommand.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "Exceptions.hpp"
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

class RemoveCommandTest : public cfgsync::tests::RegistryCommandTestFixture {};

TEST_F(RemoveCommandTest, RemovesExistingTrackedEntryAndSavesRegistry) {
    const auto sourcePath = cfgsync::utils::NormalizePath(SourcePath(".gitconfig"));
    ASSERT_TRUE(Registry().AddEntry({
        .OriginalPath = sourcePath.string(),
        .StoredRelativePath = "files/home/user/.gitconfig",
    }));
    Registry().Save();

    cfgsync::commands::RemoveCommand command{Registry()};
    command.Execute(sourcePath);

    EXPECT_TRUE(Registry().GetTrackedEntries().empty());
    const auto document = ReadJsonFile(RegistryPath());
    ASSERT_TRUE(document["tracked_files"].is_array());
    EXPECT_TRUE(document["tracked_files"].empty());
}

TEST_F(RemoveCommandTest, RemovingOneEntryDoesNotAffectOtherTrackedEntries) {
    const auto firstPath = cfgsync::utils::NormalizePath(SourcePath(".gitconfig"));
    const auto secondPath = cfgsync::utils::NormalizePath(SourcePath("init.lua"));
    ASSERT_TRUE(Registry().AddEntry({
        .OriginalPath = firstPath.string(),
        .StoredRelativePath = "files/home/user/.gitconfig",
    }));
    ASSERT_TRUE(Registry().AddEntry({
        .OriginalPath = secondPath.string(),
        .StoredRelativePath = "files/home/user/init.lua",
    }));
    Registry().Save();

    cfgsync::commands::RemoveCommand command{Registry()};
    command.Execute(firstPath);

    ASSERT_EQ(Registry().GetTrackedEntries().size(), 1U);
    EXPECT_EQ(Registry().GetTrackedEntries()[0].OriginalPath, secondPath.string());

    const auto document = ReadJsonFile(RegistryPath());
    ASSERT_EQ(document["tracked_files"].size(), 1U);
    EXPECT_EQ(document["tracked_files"][0]["original_path"], secondPath.string());
}

TEST_F(RemoveCommandTest, UsesNormalizedOriginalPathForMatching) {
    const auto sourcePath = cfgsync::utils::NormalizePath(SourcePath(".gitconfig"));
    ASSERT_TRUE(Registry().AddEntry({
        .OriginalPath = sourcePath.string(),
        .StoredRelativePath = "files/home/user/.gitconfig",
    }));
    Registry().Save();

    cfgsync::commands::RemoveCommand command{Registry()};
    command.Execute(SourcePath("..") / "user" / "." / ".gitconfig");

    EXPECT_TRUE(Registry().GetTrackedEntries().empty());
}

TEST_F(RemoveCommandTest, RemovesEntryEvenWhenOriginalFileNoLongerExists) {
    const auto sourcePath = SourcePath(".gitconfig");
    const auto normalizedSourcePath = cfgsync::utils::NormalizePath(sourcePath);
    ASSERT_FALSE(fs::exists(sourcePath));
    ASSERT_TRUE(Registry().AddEntry({
        .OriginalPath = normalizedSourcePath.string(),
        .StoredRelativePath = "files/home/user/.gitconfig",
    }));
    Registry().Save();

    cfgsync::commands::RemoveCommand command{Registry()};
    command.Execute(sourcePath);

    EXPECT_TRUE(Registry().GetTrackedEntries().empty());
    EXPECT_TRUE(ReadJsonFile(RegistryPath())["tracked_files"].empty());
}

TEST_F(RemoveCommandTest, MissingEntryFailsClearlyAndLeavesRegistryUnchanged) {
    const auto sourcePath = cfgsync::utils::NormalizePath(SourcePath(".gitconfig"));
    ASSERT_TRUE(Registry().AddEntry({
        .OriginalPath = sourcePath.string(),
        .StoredRelativePath = "files/home/user/.gitconfig",
    }));
    Registry().Save();
    const auto registryBeforeRemove = ReadJsonFile(RegistryPath());

    cfgsync::commands::RemoveCommand command{Registry()};

    try {
        command.Execute(SourcePath("missing.conf"));
        FAIL() << "Removing a missing tracked entry did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("File is not tracked"), std::string::npos);
        EXPECT_NE(message.find("missing.conf"), std::string::npos);
    }

    ASSERT_EQ(Registry().GetTrackedEntries().size(), 1U);
    EXPECT_EQ(Registry().GetTrackedEntries()[0].OriginalPath, sourcePath.string());
    EXPECT_EQ(ReadJsonFile(RegistryPath()), registryBeforeRemove);
}

TEST_F(RemoveCommandTest, DoesNotDeleteStoredBackupFile) {
    const auto sourcePath = cfgsync::utils::NormalizePath(SourcePath(".gitconfig"));
    const fs::path storedRelativePath{"files/home/user/.gitconfig"};
    const auto storedPath = StorageRoot() / storedRelativePath;
    WriteTextFile(storedPath, "[user]\n");
    ASSERT_TRUE(Registry().AddEntry({
        .OriginalPath = sourcePath.string(),
        .StoredRelativePath = storedRelativePath.generic_string(),
    }));
    Registry().Save();

    cfgsync::commands::RemoveCommand command{Registry()};
    command.Execute(sourcePath);

    EXPECT_TRUE(fs::exists(storedPath));
    EXPECT_EQ(ReadJsonFile(RegistryPath())["tracked_files"].size(), 0U);
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
