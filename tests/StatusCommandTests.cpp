#include "commands/StatusCommand.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "gtest/gtest.h"
#include "storage/StorageManager.hpp"

#include <filesystem>
#include <sstream>
#include <string>

namespace {
namespace fs = std::filesystem;
using cfgsync::tests::TrackFile;

class StatusCommandTest : public cfgsync::tests::RegistryCommandTestFixture {
protected:
    std::string RunStatusCommand() {
        cfgsync::storage::StorageManager storageManager{StorageRoot()};
        const cfgsync::commands::StatusCommand command{Registry(), storageManager};
        std::ostringstream output;
        command.Execute(output);
        return output.str();
    }
};

TEST_F(StatusCommandTest, EmptyRegistryPrintsClean) { EXPECT_EQ(RunStatusCommand(), "Clean.\n"); }

TEST_F(StatusCommandTest, CleanTrackedFilePrintsClean) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "[user]\n");

    EXPECT_EQ(RunStatusCommand(), "Clean.\n");
}

TEST_F(StatusCommandTest, ModifiedTrackedFileIsPrinted) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");

    EXPECT_EQ(RunStatusCommand(), "modified " + Registry().GetTrackedEntries().front().OriginalPath + "\n");
}

TEST_F(StatusCommandTest, MissingOriginalIsPrinted) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");

    EXPECT_EQ(RunStatusCommand(), "missing-original " + Registry().GetTrackedEntries().front().OriginalPath + "\n");
}

TEST_F(StatusCommandTest, MissingBackupIsPrinted) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "local contents\n");
    TrackFile(Registry(), sourcePath);

    EXPECT_EQ(RunStatusCommand(), "missing-backup " + Registry().GetTrackedEntries().front().OriginalPath + "\n");
}

TEST_F(StatusCommandTest, MixedStatusesPreserveRegistryOrderAndSkipCleanEntries) {
    const auto modifiedPath = SourcePath(".gitconfig");
    const auto cleanPath = SourcePath("init.lua");
    const auto missingBackupPath = SourcePath("missing-backup.conf");
    cfgsync::tests::WriteTextFile(modifiedPath, "local\n");
    const auto modifiedStoredRelativePath = TrackFile(Registry(), modifiedPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / modifiedStoredRelativePath, "stored\n");
    cfgsync::tests::WriteTextFile(cleanPath, "same\n");
    const auto cleanStoredRelativePath = TrackFile(Registry(), cleanPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / cleanStoredRelativePath, "same\n");
    cfgsync::tests::WriteTextFile(missingBackupPath, "local\n");
    TrackFile(Registry(), missingBackupPath);

    const auto& entries = Registry().GetTrackedEntries();

    EXPECT_EQ(RunStatusCommand(),
              "modified " + entries[0].OriginalPath + "\nmissing-backup " + entries[2].OriginalPath + "\n");
}

TEST_F(StatusCommandTest, ExecuteDoesNotMutateOrSaveRegistry) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "local contents\n");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");
    const auto registryBeforeStatus = cfgsync::tests::ReadJsonFile(RegistryPath());

    const auto output = RunStatusCommand();

    EXPECT_EQ(output, "modified " + Registry().GetTrackedEntries().front().OriginalPath + "\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeStatus);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
