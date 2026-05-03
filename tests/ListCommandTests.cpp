#include "commands/ListCommand.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "gtest/gtest.h"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
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

class ListCommandTest : public cfgsync::tests::RegistryCommandTestFixture {};

TEST_F(ListCommandTest, EmptyRegistryPrintsConciseMessage) {
    const cfgsync::commands::ListCommand command{Registry()};
    std::ostringstream output;

    command.Execute(output);

    EXPECT_EQ(output.str(), "No files tracked.\n");
}

TEST_F(ListCommandTest, NonEmptyRegistryPrintsOriginalPathsInRegistryOrder) {
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
    const cfgsync::commands::ListCommand command{Registry()};
    std::ostringstream output;

    command.Execute(output);

    EXPECT_EQ(output.str(), firstPath.string() + "\n" + secondPath.string() + "\n");
}

TEST_F(ListCommandTest, ExecuteDoesNotMutateOrSaveRegistry) {
    const auto sourcePath = cfgsync::utils::NormalizePath(SourcePath(".gitconfig"));
    ASSERT_TRUE(Registry().AddEntry({
        .OriginalPath = sourcePath.string(),
        .StoredRelativePath = "files/home/user/.gitconfig",
    }));
    Registry().Save();
    const auto registryBeforeList = ReadJsonFile(RegistryPath());

    WriteTextFile(RegistryPath(), registryBeforeList.dump(4) + "\n");
    const cfgsync::commands::ListCommand command{Registry()};
    std::ostringstream output;

    command.Execute(output);

    EXPECT_EQ(ReadJsonFile(RegistryPath()), registryBeforeList);
    ASSERT_EQ(Registry().GetTrackedEntries().size(), 1U);
    EXPECT_EQ(Registry().GetTrackedEntries()[0].OriginalPath, sourcePath.string());
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
