#include "common/GoogleTestMain.hpp"
#include "common/TestFileUtils.hpp"
#include "common/TestTempDirectory.hpp"
#include "core/TrackedEntry.hpp"
#include "diff/FileComparator.hpp"
#include "gtest/gtest.h"
#include "storage/StorageManager.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>

namespace {
namespace fs = std::filesystem;
using enum cfgsync::diff::FileStatus;

class FileComparatorTest : public testing::Test {
protected:
    void SetUp() override { TestRoot_ = cfgsync::tests::MakeTestRoot(); }

    void TearDown() override { fs::remove_all(TestRoot_); }

    fs::path SourcePath(const std::string& filename = ".gitconfig") const {
        return TestRoot_ / "home" / "user" / filename;
    }

    fs::path StorageRoot() const { return TestRoot_ / "storage"; }

    cfgsync::core::TrackedEntry TrackedEntryFor(const fs::path& sourcePath) const {
        const auto normalizedSourcePath = cfgsync::utils::NormalizePath(sourcePath);
        return {
            .OriginalPath = normalizedSourcePath.string(),
            .StoredRelativePath = cfgsync::utils::MakeStorageRelativePath(normalizedSourcePath).generic_string(),
        };
    }

private:
    fs::path TestRoot_;
};

TEST_F(FileComparatorTest, ReportsCleanWhenOriginalAndBackupBytesMatch) {
    const auto sourcePath = SourcePath();
    const auto entry = TrackedEntryFor(sourcePath);
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};
    cfgsync::tests::WriteTextFile(sourcePath, "[user]\n");
    cfgsync::tests::WriteTextFile(storageManager.ResolveStoredPath(entry), "[user]\n");

    const cfgsync::diff::FileComparator comparator{storageManager};
    const auto result = comparator.Compare(entry);

    EXPECT_EQ(result.Status, Clean);
    EXPECT_EQ(result.Entry.OriginalPath, entry.OriginalPath);
}

TEST_F(FileComparatorTest, ReportsModifiedWhenBytesDiffer) {
    const auto sourcePath = SourcePath();
    const auto entry = TrackedEntryFor(sourcePath);
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};
    cfgsync::tests::WriteTextFile(sourcePath, "local changes\n");
    cfgsync::tests::WriteTextFile(storageManager.ResolveStoredPath(entry), "stored contents\n");

    const cfgsync::diff::FileComparator comparator{storageManager};
    const auto result = comparator.Compare(entry);

    EXPECT_EQ(result.Status, Modified);
}

TEST_F(FileComparatorTest, ReportsMissingOriginalWhenOriginalFileDoesNotExist) {
    const auto sourcePath = SourcePath();
    const auto entry = TrackedEntryFor(sourcePath);
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};
    cfgsync::tests::WriteTextFile(storageManager.ResolveStoredPath(entry), "stored contents\n");

    const cfgsync::diff::FileComparator comparator{storageManager};
    const auto result = comparator.Compare(entry);

    EXPECT_EQ(result.Status, MissingOriginal);
}

TEST_F(FileComparatorTest, ReportsMissingBackupWhenStoredFileDoesNotExist) {
    const auto sourcePath = SourcePath();
    const auto entry = TrackedEntryFor(sourcePath);
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};
    cfgsync::tests::WriteTextFile(sourcePath, "local contents\n");

    const cfgsync::diff::FileComparator comparator{storageManager};
    const auto result = comparator.Compare(entry);

    EXPECT_EQ(result.Status, MissingBackup);
}

TEST_F(FileComparatorTest, BothFilesMissingPrefersMissingOriginal) {
    const auto entry = TrackedEntryFor(SourcePath());
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};

    const cfgsync::diff::FileComparator comparator{storageManager};
    const auto result = comparator.Compare(entry);

    EXPECT_EQ(result.Status, MissingOriginal);
}

TEST_F(FileComparatorTest, MultipleEntriesRetainInputOrder) {
    const auto firstPath = SourcePath(".gitconfig");
    const auto secondPath = SourcePath("init.lua");
    const auto firstEntry = TrackedEntryFor(firstPath);
    const auto secondEntry = TrackedEntryFor(secondPath);
    const cfgsync::storage::StorageManager storageManager{StorageRoot()};
    cfgsync::tests::WriteTextFile(firstPath, "changed\n");
    cfgsync::tests::WriteTextFile(storageManager.ResolveStoredPath(firstEntry), "stored\n");
    cfgsync::tests::WriteTextFile(secondPath, "same\n");
    cfgsync::tests::WriteTextFile(storageManager.ResolveStoredPath(secondEntry), "same\n");

    const cfgsync::diff::FileComparator comparator{storageManager};
    const auto results = comparator.CompareAll({firstEntry, secondEntry});

    ASSERT_EQ(results.size(), 2U);
    EXPECT_EQ(results[0].Entry.OriginalPath, firstEntry.OriginalPath);
    EXPECT_EQ(results[0].Status, Modified);
    EXPECT_EQ(results[1].Entry.OriginalPath, secondEntry.OriginalPath);
    EXPECT_EQ(results[1].Status, Clean);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunGoogleTests(argc, argv); }
