#include "Exceptions.hpp"
#include "commands/RestoreCommand.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/RegistryCommandTestFixture.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestRegistryUtils.hpp"
#include "gtest/gtest.h"
#include "storage/StorageManager.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;
using cfgsync::tests::TrackFile;

class RestoreCommandTest : public cfgsync::tests::RegistryCommandTestFixture {};

TEST_F(RestoreCommandTest, RestoresOneTrackedFile) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(sourcePath, "changed contents\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteSingle(sourcePath);

    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "[user]\n");
}

TEST_F(RestoreCommandTest, RestoresAllTrackedFiles) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("init.lua");
    const auto firstStoredRelativePath = TrackFile(Registry(), firstPath);
    const auto secondStoredRelativePath = TrackFile(Registry(), secondPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / firstStoredRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / secondStoredRelativePath, "vim.opt.number = true\n");
    cfgsync::tests::WriteTextFile(firstPath, "changed first\n");
    cfgsync::tests::WriteTextFile(secondPath, "changed second\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteAll();

    EXPECT_EQ(cfgsync::tests::ReadTextFile(firstPath), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(secondPath), "vim.opt.number = true\n");
}

TEST_F(RestoreCommandTest, CreatesDestinationParentDirectories) {
    const auto sourcePath = SourcePath(".config/nvim/init.lua");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "vim.opt.number = true\n");
    ASSERT_FALSE(fs::exists(sourcePath.parent_path()));

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteSingle(sourcePath);

    EXPECT_TRUE(fs::exists(sourcePath));
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "vim.opt.number = true\n");
}

TEST_F(RestoreCommandTest, OverwritesChangedLocalFile) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteSingle(sourcePath);

    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "stored contents\n");
}

TEST_F(RestoreCommandTest, SingleDryRunDoesNotOverwriteDestination) {
    const auto sourcePath = SourcePath(".gitconfig");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    testing::internal::CaptureStdout();
    command.ExecuteSingle(sourcePath, std::nullopt, cfgsync::commands::RestoreMode::DryRun);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("would-overwrite " + sourcePath.string()), std::string::npos);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "local changes\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeRestore);
}

TEST_F(RestoreCommandTest, SingleDryRunReportsOverwriteWhenSameLengthContentsDiffer) {
    const auto sourcePath = SourcePath(".gitconfig");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored value\n");
    cfgsync::tests::WriteTextFile(sourcePath, "localx value\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    testing::internal::CaptureStdout();
    command.ExecuteSingle(sourcePath, std::nullopt, cfgsync::commands::RestoreMode::DryRun);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("would-overwrite " + sourcePath.string()), std::string::npos);
    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "localx value\n");
}

TEST_F(RestoreCommandTest, SingleDryRunFailsWhenDestinationIsDirectory) {
    const auto sourcePath = SourcePath(".gitconfig");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "stored contents\n");
    fs::create_directories(sourcePath);

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteSingle(sourcePath, std::nullopt, cfgsync::commands::RestoreMode::DryRun);
        FAIL() << "Dry-run restore to a directory destination did not throw.";
    } catch (const cfgsync::FileError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Destination path is not an ordinary file"), std::string::npos);
        EXPECT_NE(message.find(sourcePath.string()), std::string::npos);
    }

    EXPECT_TRUE(fs::is_directory(sourcePath));
}

TEST_F(RestoreCommandTest, RestoreAllDryRunReportsCreateOverwriteAndUnchangedWithoutMutatingDestinations) {
    const auto createPath = SourcePath("missing.conf");
    const auto overwritePath = SourcePath(".gitconfig");
    const auto unchangedPath = SourcePath("init.lua");
    const auto createStoredRelativePath = TrackFile(Registry(), createPath);
    const auto overwriteStoredRelativePath = TrackFile(Registry(), overwritePath);
    const auto unchangedStoredRelativePath = TrackFile(Registry(), unchangedPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / createStoredRelativePath, "created contents\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / overwriteStoredRelativePath, "stored contents\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / unchangedStoredRelativePath, "same contents\n");
    cfgsync::tests::WriteTextFile(overwritePath, "local changes\n");
    cfgsync::tests::WriteTextFile(unchangedPath, "same contents\n");
    ASSERT_FALSE(fs::exists(createPath));
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    testing::internal::CaptureStdout();
    command.ExecuteAll(std::nullopt, cfgsync::commands::RestoreMode::DryRun);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("would-create " + createPath.string()), std::string::npos);
    EXPECT_NE(output.find("would-overwrite " + overwritePath.string()), std::string::npos);
    EXPECT_NE(output.find("unchanged " + unchangedPath.string()), std::string::npos);
    EXPECT_FALSE(fs::exists(createPath));
    EXPECT_EQ(cfgsync::tests::ReadTextFile(overwritePath), "local changes\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(unchangedPath), "same contents\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeRestore);
}

TEST_F(RestoreCommandTest, SingleRestoreWithPrefixRemapRestoresToRemappedDestination) {
    const auto sourcePath = SourcePath(".config/nvim/init.lua");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    const auto fromPrefix = sourcePath.parent_path().parent_path().parent_path();
    const auto toPrefix = StorageRoot().parent_path() / "new-home" / "user";
    const auto destinationPath = toPrefix / ".config" / "nvim" / "init.lua";
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "vim.opt.number = true\n");
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteSingle(sourcePath, cfgsync::commands::RestorePrefixRemap{
                                          .FromPrefix = cfgsync::utils::NormalizePath(fromPrefix),
                                          .ToPrefix = cfgsync::utils::NormalizePath(toPrefix),
                                      });

    EXPECT_EQ(cfgsync::tests::ReadTextFile(destinationPath), "vim.opt.number = true\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeRestore);
}

TEST_F(RestoreCommandTest, RestoreAllWithPrefixRemapRestoresMultipleFilesToRemappedDestinations) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath(".config/nvim/init.lua");
    const auto firstStoredRelativePath = TrackFile(Registry(), firstPath);
    const auto secondStoredRelativePath = TrackFile(Registry(), secondPath);
    const auto fromPrefix = SourcePath().parent_path();
    const auto toPrefix = StorageRoot().parent_path() / "new-home" / "user";
    cfgsync::tests::WriteTextFile(StorageRoot() / firstStoredRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / secondStoredRelativePath, "vim.opt.number = true\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    command.ExecuteAll(cfgsync::commands::RestorePrefixRemap{
        .FromPrefix = cfgsync::utils::NormalizePath(fromPrefix),
        .ToPrefix = cfgsync::utils::NormalizePath(toPrefix),
    });

    EXPECT_EQ(cfgsync::tests::ReadTextFile(toPrefix / ".gitconfig"), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadTextFile(toPrefix / ".config" / "nvim" / "init.lua"), "vim.opt.number = true\n");
}

TEST_F(RestoreCommandTest, DryRunWithPrefixRemapReportsRemappedDestinationWithoutCreatingParents) {
    const auto sourcePath = SourcePath(".config/nvim/init.lua");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    const auto fromPrefix = SourcePath().parent_path();
    const auto toPrefix = StorageRoot().parent_path() / "new-home" / "user";
    const auto destinationPath = toPrefix / ".config" / "nvim" / "init.lua";
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "vim.opt.number = true\n");
    ASSERT_FALSE(fs::exists(toPrefix));

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    testing::internal::CaptureStdout();
    command.ExecuteSingle(sourcePath,
                          cfgsync::commands::RestorePrefixRemap{
                              .FromPrefix = cfgsync::utils::NormalizePath(fromPrefix),
                              .ToPrefix = cfgsync::utils::NormalizePath(toPrefix),
                          },
                          cfgsync::commands::RestoreMode::DryRun);
    const auto output = testing::internal::GetCapturedStdout();

    EXPECT_NE(output.find("would-create " + destinationPath.string()), std::string::npos);
    EXPECT_FALSE(fs::exists(destinationPath));
    EXPECT_FALSE(fs::exists(toPrefix));
}

TEST_F(RestoreCommandTest, SingleRestoreWithPrefixRemapFailsWhenTrackedFileIsOutsidePrefix) {
    const auto sourcePath = SourcePath(".gitconfig");
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);
    const auto fromPrefix = StorageRoot().parent_path() / "other-home" / "user";
    const auto toPrefix = StorageRoot().parent_path() / "new-home" / "user";
    cfgsync::tests::WriteTextFile(StorageRoot() / storedRelativePath, "[user]\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteSingle(sourcePath, cfgsync::commands::RestorePrefixRemap{
                                              .FromPrefix = cfgsync::utils::NormalizePath(fromPrefix),
                                              .ToPrefix = cfgsync::utils::NormalizePath(toPrefix),
                                          });
        FAIL() << "Restore with a non-matching prefix did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("outside --from-prefix"), std::string::npos);
        EXPECT_NE(message.find(cfgsync::utils::NormalizePath(sourcePath).string()), std::string::npos);
    }
}

TEST_F(RestoreCommandTest, SingleRestoreFailsForUntrackedFile) {
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteSingle(SourcePath());
        FAIL() << "Restore of an untracked file did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("File is not tracked"), std::string::npos);
        EXPECT_NE(message.find(cfgsync::utils::NormalizePath(SourcePath()).string()), std::string::npos);
    }
}

TEST_F(RestoreCommandTest, SingleRestoreFailsWhenStoredBackupIsMissing) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteSingle(sourcePath);
        FAIL() << "Restore with a missing stored backup did not throw.";
    } catch (const cfgsync::FileError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Path does not exist"), std::string::npos);
        EXPECT_NE(message.find((StorageRoot() / storedRelativePath).string()), std::string::npos);
    }
}

TEST_F(RestoreCommandTest, SingleDryRunFailsWhenStoredBackupIsMissing) {
    const auto sourcePath = SourcePath();
    const auto storedRelativePath = TrackFile(Registry(), sourcePath);

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteSingle(sourcePath, std::nullopt, cfgsync::commands::RestoreMode::DryRun);
        FAIL() << "Dry-run restore with a missing stored backup did not throw.";
    } catch (const cfgsync::FileError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Path does not exist"), std::string::npos);
        EXPECT_NE(message.find((StorageRoot() / storedRelativePath).string()), std::string::npos);
    }
}

TEST_F(RestoreCommandTest, RestoreAllDryRunContinuesAfterMissingStoredBackupAndReportsPartialFailure) {
    const auto missingBackupPath = SourcePath("missing.conf");
    const auto previewPath = SourcePath(".gitconfig");
    TrackFile(Registry(), missingBackupPath);
    const auto previewStoredRelativePath = TrackFile(Registry(), previewPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / previewStoredRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(previewPath, "changed contents\n");
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    testing::internal::CaptureStdout();
    try {
        command.ExecuteAll(std::nullopt, cfgsync::commands::RestoreMode::DryRun);
        FAIL() << "Dry-run restore with a missing stored backup did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const auto output = testing::internal::GetCapturedStdout();
        const std::string message = error.what();
        EXPECT_NE(output.find("Failed to restore file"), std::string::npos);
        EXPECT_NE(output.find(missingBackupPath.string()), std::string::npos);
        EXPECT_NE(output.find("would-overwrite " + previewPath.string()), std::string::npos);
        EXPECT_NE(message.find("Restore completed with 1 failure."), std::string::npos);
    }

    EXPECT_EQ(cfgsync::tests::ReadTextFile(previewPath), "changed contents\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeRestore);
}

TEST_F(RestoreCommandTest, RestoreAllDryRunContinuesAfterDestinationStateFailure) {
    const auto directoryDestinationPath = SourcePath(".gitconfig");
    const auto previewPath = SourcePath("init.lua");
    const auto directoryStoredRelativePath = TrackFile(Registry(), directoryDestinationPath);
    const auto previewStoredRelativePath = TrackFile(Registry(), previewPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / directoryStoredRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / previewStoredRelativePath, "vim.opt.number = true\n");
    fs::create_directories(directoryDestinationPath);
    ASSERT_FALSE(fs::exists(previewPath));

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    testing::internal::CaptureStdout();
    try {
        command.ExecuteAll(std::nullopt, cfgsync::commands::RestoreMode::DryRun);
        FAIL() << "Dry-run restore with an invalid destination did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const auto output = testing::internal::GetCapturedStdout();
        const std::string message = error.what();
        EXPECT_NE(output.find("Failed to restore file"), std::string::npos);
        EXPECT_NE(output.find("Destination path is not an ordinary file"), std::string::npos);
        EXPECT_NE(output.find("would-create " + previewPath.string()), std::string::npos);
        EXPECT_NE(message.find("Restore completed with 1 failure."), std::string::npos);
    }

    EXPECT_TRUE(fs::is_directory(directoryDestinationPath));
    EXPECT_FALSE(fs::exists(previewPath));
}

TEST_F(RestoreCommandTest, RestoreAllContinuesAfterMissingStoredBackupAndReportsPartialFailure) {
    const auto existingPath = SourcePath(".gitconfig");
    const auto missingBackupPath = SourcePath("missing.conf");
    const auto existingStoredRelativePath = TrackFile(Registry(), existingPath);
    TrackFile(Registry(), missingBackupPath);
    cfgsync::tests::WriteTextFile(StorageRoot() / existingStoredRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(existingPath, "changed contents\n");
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteAll();
        FAIL() << "Restore with a missing stored backup did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Restore completed with 1 failure."), std::string::npos);
    }

    EXPECT_EQ(cfgsync::tests::ReadTextFile(existingPath), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeRestore);
}

TEST_F(RestoreCommandTest, RestoreAllWithPrefixRemapContinuesWhenEntryIsOutsidePrefix) {
    const auto restoredPath = SourcePath(".gitconfig");
    const auto outsidePath = StorageRoot().parent_path() / "other-home" / "user" / "settings.conf";
    const auto restoredStoredRelativePath = TrackFile(Registry(), restoredPath);
    const auto outsideStoredRelativePath = TrackFile(Registry(), outsidePath);
    const auto fromPrefix = SourcePath().parent_path();
    const auto toPrefix = StorageRoot().parent_path() / "new-home" / "user";
    cfgsync::tests::WriteTextFile(StorageRoot() / restoredStoredRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / outsideStoredRelativePath, "outside\n");
    const auto registryBeforeRestore = cfgsync::tests::ReadJsonFile(RegistryPath());

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    try {
        command.ExecuteAll(cfgsync::commands::RestorePrefixRemap{
            .FromPrefix = cfgsync::utils::NormalizePath(fromPrefix),
            .ToPrefix = cfgsync::utils::NormalizePath(toPrefix),
        });
        FAIL() << "Restore with a non-matching prefix did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Restore completed with 1 failure."), std::string::npos);
    }

    EXPECT_EQ(cfgsync::tests::ReadTextFile(toPrefix / ".gitconfig"), "[user]\n");
    EXPECT_EQ(cfgsync::tests::ReadJsonFile(RegistryPath()), registryBeforeRestore);
}

TEST_F(RestoreCommandTest, RestoreAllDryRunWithPrefixRemapContinuesWhenEntryIsOutsidePrefix) {
    const auto restoredPath = SourcePath(".gitconfig");
    const auto outsidePath = StorageRoot().parent_path() / "other-home" / "user" / "settings.conf";
    const auto restoredStoredRelativePath = TrackFile(Registry(), restoredPath);
    const auto outsideStoredRelativePath = TrackFile(Registry(), outsidePath);
    const auto fromPrefix = SourcePath().parent_path();
    const auto toPrefix = StorageRoot().parent_path() / "new-home" / "user";
    cfgsync::tests::WriteTextFile(StorageRoot() / restoredStoredRelativePath, "[user]\n");
    cfgsync::tests::WriteTextFile(StorageRoot() / outsideStoredRelativePath, "outside\n");

    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    testing::internal::CaptureStdout();
    try {
        command.ExecuteAll(
            cfgsync::commands::RestorePrefixRemap{
                .FromPrefix = cfgsync::utils::NormalizePath(fromPrefix),
                .ToPrefix = cfgsync::utils::NormalizePath(toPrefix),
            },
            cfgsync::commands::RestoreMode::DryRun);
        FAIL() << "Dry-run restore with a non-matching prefix did not throw.";
    } catch (const cfgsync::CommandError& error) {
        const auto output = testing::internal::GetCapturedStdout();
        const std::string message = error.what();
        EXPECT_NE(output.find("would-create " + (toPrefix / ".gitconfig").string()), std::string::npos);
        EXPECT_NE(output.find("Failed to restore file"), std::string::npos);
        EXPECT_NE(message.find("Restore completed with 1 failure."), std::string::npos);
    }

    EXPECT_FALSE(fs::exists(toPrefix / ".gitconfig"));
}

TEST_F(RestoreCommandTest, EmptyRegistrySucceedsWithoutCreatingStoredFiles) {
    cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::commands::RestoreCommand command{Registry(), storageManager};

    EXPECT_NO_THROW(command.ExecuteAll());
    EXPECT_FALSE(fs::exists(StorageRoot() / "files"));
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
