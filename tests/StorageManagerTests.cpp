#include "common/GoogleTestMain.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestTempDirectory.hpp"
#include "core/TrackedEntry.hpp"
#include "storage/StorageManager.hpp"

#include "gtest/gtest.h"

#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;

class StorageManagerTest : public testing::Test {
protected:
    void SetUp() override { TestRoot_ = cfgsync::tests::MakeTestRoot(); }

    void TearDown() override { fs::remove_all(TestRoot_); }

    fs::path SourcePath(const std::string& filename = ".gitconfig") const {
        return TestRoot_ / "home" / "user" / filename;
    }

    fs::path StorageRoot() const { return TestRoot_ / "storage"; }

    cfgsync::core::TrackedEntry TrackedEntryFor(const fs::path& sourcePath) const {
        const auto storedRelativePath =
            cfgsync::tests::StoredPathFor(StorageRoot(), sourcePath).lexically_relative(StorageRoot());

        return {
            .OriginalPath = sourcePath.string(),
            .StoredRelativePath = storedRelativePath.generic_string(),
        };
    }

private:
    fs::path TestRoot_;
};

TEST_F(StorageManagerTest, ResolvesRegistryPathUnderStorageRoot) {
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};

    EXPECT_EQ(storageManager.GetRegistryPath(), StorageRoot() / "registry.json");
}

TEST_F(StorageManagerTest, EmptyStorageRootResolvesEmptyPaths) {
    const cfgsync::storage::StorageManager storageManager;
    const cfgsync::core::TrackedEntry entry{
        .OriginalPath = SourcePath().string(),
        .StoredRelativePath = "files/home/user/.gitconfig",
    };

    EXPECT_TRUE(storageManager.GetRegistryPath().empty());
    EXPECT_TRUE(storageManager.ResolveStoredPath(entry).empty());
}

TEST_F(StorageManagerTest, ResolvesStoredPathFromTrackedRelativePath) {
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};
    const cfgsync::core::TrackedEntry entry{
        .OriginalPath = SourcePath().string(),
        .StoredRelativePath = "files/home/user/.gitconfig",
    };

    EXPECT_EQ(storageManager.ResolveStoredPath(entry), StorageRoot() / "files/home/user/.gitconfig");
}

TEST_F(StorageManagerTest, BackupCopiesSourceToResolvedStoredPath) {
    const auto sourcePath = SourcePath();
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    const auto entry = TrackedEntryFor(sourcePath);
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};

    storageManager.BackupEntry(entry);

    EXPECT_EQ(cfgsync::tests::ReadTextFile(storageManager.ResolveStoredPath(entry)), "[user]\n");
}

TEST_F(StorageManagerTest, RestoreCopiesStoredPathToOriginalDestination) {
    const auto sourcePath = SourcePath(".config/nvim/init.lua");
    const auto entry = TrackedEntryFor(sourcePath);
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};
    cfgsync::tests::WriteTextFile(storageManager.ResolveStoredPath(entry), "vim.opt.number = true\n");

    storageManager.RestoreEntry(entry);

    EXPECT_EQ(cfgsync::tests::ReadTextFile(sourcePath), "vim.opt.number = true\n");
}

}  // namespace

int main(int argc, char** argv) {
    return cfgsync::tests::RunGoogleTests(argc, argv);
}
