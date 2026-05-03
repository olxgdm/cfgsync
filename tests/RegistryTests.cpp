#include "Exceptions.hpp"
#include "common/TestTempDirectory.hpp"
#include "core/Registry.hpp"
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

void WriteJsonFile(const fs::path& path, const nlohmann::json& document) {
    WriteTextFile(path, document.dump(4) + "\n");
}

class RegistryTest : public testing::Test {
protected:
    void SetUp() override { TestRoot = cfgsync::tests::MakeTestRoot(); }

    void TearDown() override { fs::remove_all(TestRoot); }

    fs::path StorageRoot() const { return TestRoot / "storage"; }

    fs::path RegistryPath() const { return StorageRoot() / "registry.json"; }

private:
    fs::path TestRoot;
};

TEST_F(RegistryTest, SavesEmptyVersionOneRegistry) {
    cfgsync::core::Registry registry{RegistryPath()};
    registry.SetStorageRoot(StorageRoot());

    registry.Save();

    const auto document = ReadJsonFile(RegistryPath());
    EXPECT_EQ(document["version"], 1);
    EXPECT_EQ(document["storage_root"], cfgsync::utils::NormalizePath(StorageRoot()).string());
    ASSERT_TRUE(document["tracked_files"].is_array());
    EXPECT_TRUE(document["tracked_files"].empty());
}

TEST_F(RegistryTest, LoadsValidVersionOneRegistry) {
    const auto normalizedStorageRoot = cfgsync::utils::NormalizePath(StorageRoot());
    const auto originalPath = cfgsync::utils::NormalizePath(StorageRoot() / "source.conf");
    WriteJsonFile(RegistryPath(), {
                                      {"version", 1},
                                      {"storage_root", normalizedStorageRoot.string()},
                                      {"tracked_files", nlohmann::json::array({
                                                            {
                                                                {"original_path", originalPath.string()},
                                                                {"stored_relative_path", "files/source.conf"},
                                                            },
                                                        })},
                                  });

    cfgsync::core::Registry registry{RegistryPath()};
    registry.Load();

    EXPECT_EQ(registry.GetStorageRoot(), normalizedStorageRoot);
    ASSERT_EQ(registry.GetTrackedEntries().size(), 1U);
    EXPECT_EQ(registry.GetTrackedEntries()[0].OriginalPath, originalPath.string());
    EXPECT_EQ(registry.GetTrackedEntries()[0].StoredRelativePath, "files/source.conf");
}

TEST_F(RegistryTest, SavesAndLoadsRoundTripWithTrackedEntries) {
    cfgsync::core::Registry saved{RegistryPath()};
    saved.SetStorageRoot(StorageRoot());
    ASSERT_TRUE(saved.AddEntry({
        .OriginalPath = (StorageRoot() / "." / "source.conf").string(),
        .StoredRelativePath = "files/source.conf",
    }));
    saved.Save();

    cfgsync::core::Registry loaded{RegistryPath()};
    loaded.Load();

    ASSERT_EQ(loaded.GetTrackedEntries().size(), 1U);
    EXPECT_EQ(loaded.GetTrackedEntries()[0].OriginalPath,
              cfgsync::utils::NormalizePath(StorageRoot() / "source.conf").string());
    EXPECT_EQ(loaded.GetTrackedEntries()[0].StoredRelativePath, "files/source.conf");
}

TEST_F(RegistryTest, MalformedJsonThrowsClearError) {
    WriteTextFile(RegistryPath(), "{ invalid json");

    cfgsync::core::Registry registry{RegistryPath()};

    try {
        registry.Load();
        FAIL() << "Malformed registry did not throw.";
    } catch (const cfgsync::RegistryError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Malformed cfgsync registry"), std::string::npos);
    }
}

TEST_F(RegistryTest, MissingRegistryFileThrowsClearError) {
    cfgsync::core::Registry registry{RegistryPath()};

    try {
        registry.Load();
        FAIL() << "Missing registry file did not throw.";
    } catch (const cfgsync::RegistryError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Unable to open cfgsync registry"), std::string::npos);
    }
}

TEST_F(RegistryTest, MissingTrackedFilesThrowsClearError) {
    WriteJsonFile(RegistryPath(), {
                                      {"version", 1},
                                      {"storage_root", cfgsync::utils::NormalizePath(StorageRoot()).string()},
                                  });

    cfgsync::core::Registry registry{RegistryPath()};

    try {
        registry.Load();
        FAIL() << "Registry without tracked_files did not throw.";
    } catch (const cfgsync::RegistryError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("tracked_files must be an array"), std::string::npos);
    }
}

TEST_F(RegistryTest, UnsupportedVersionThrowsClearError) {
    WriteJsonFile(RegistryPath(), {
                                      {"version", 999},
                                      {"storage_root", cfgsync::utils::NormalizePath(StorageRoot()).string()},
                                      {"tracked_files", nlohmann::json::array()},
                                  });

    cfgsync::core::Registry registry{RegistryPath()};

    try {
        registry.Load();
        FAIL() << "Unsupported registry version did not throw.";
    } catch (const cfgsync::RegistryError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Unsupported cfgsync registry version"), std::string::npos);
    }
}

TEST_F(RegistryTest, DuplicateOriginalPathsInFileThrowClearError) {
    const auto normalizedStorageRoot = cfgsync::utils::NormalizePath(StorageRoot());
    const auto originalPath = cfgsync::utils::NormalizePath(StorageRoot() / "source.conf");
    WriteJsonFile(RegistryPath(),
                  {
                      {"version", 1},
                      {"storage_root", normalizedStorageRoot.string()},
                      {"tracked_files", nlohmann::json::array({
                                            {
                                                {"original_path", originalPath.string()},
                                                {"stored_relative_path", "files/source.conf"},
                                            },
                                            {
                                                {"original_path", (StorageRoot() / "." / "source.conf").string()},
                                                {"stored_relative_path", "files/duplicate.conf"},
                                            },
                                        })},
                  });

    cfgsync::core::Registry registry{RegistryPath()};

    try {
        registry.Load();
        FAIL() << "Duplicate registry entries did not throw.";
    } catch (const cfgsync::RegistryError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("duplicate original_path"), std::string::npos);
    }
}

TEST_F(RegistryTest, AddEntryPreventsDuplicates) {
    cfgsync::core::Registry registry{RegistryPath()};
    registry.SetStorageRoot(StorageRoot());

    EXPECT_TRUE(registry.AddEntry({
        .OriginalPath = (StorageRoot() / "source.conf").string(),
        .StoredRelativePath = "files/source.conf",
    }));
    EXPECT_FALSE(registry.AddEntry({
        .OriginalPath = (StorageRoot() / "." / "source.conf").string(),
        .StoredRelativePath = "files/source-copy.conf",
    }));

    EXPECT_EQ(registry.GetTrackedEntries().size(), 1U);
}

TEST_F(RegistryTest, RemoveExistingEntryIsDeterministic) {
    cfgsync::core::Registry registry{RegistryPath()};
    registry.SetStorageRoot(StorageRoot());
    ASSERT_TRUE(registry.AddEntry({
        .OriginalPath = (StorageRoot() / "source.conf").string(),
        .StoredRelativePath = "files/source.conf",
    }));

    EXPECT_TRUE(registry.RemoveEntry(StorageRoot() / "." / "source.conf"));
    EXPECT_TRUE(registry.GetTrackedEntries().empty());
    EXPECT_FALSE(registry.RemoveEntry(StorageRoot() / "source.conf"));
}

TEST_F(RegistryTest, RemoveMissingEntryReturnsFalse) {
    cfgsync::core::Registry registry{RegistryPath()};
    registry.SetStorageRoot(StorageRoot());

    EXPECT_FALSE(registry.RemoveEntry(StorageRoot() / "missing.conf"));
}

TEST_F(RegistryTest, LookupUsesNormalizedOriginalPath) {
    cfgsync::core::Registry registry{RegistryPath()};
    registry.SetStorageRoot(StorageRoot());
    ASSERT_TRUE(registry.AddEntry({
        .OriginalPath = (StorageRoot() / "source.conf").string(),
        .StoredRelativePath = "files/source.conf",
    }));

    const auto* entry = registry.FindEntryByOriginalPath(StorageRoot() / "." / "source.conf");

    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->StoredRelativePath, "files/source.conf");
    EXPECT_TRUE(registry.ContainsOriginalPath(StorageRoot() / "." / "source.conf"));
    EXPECT_FALSE(registry.ContainsOriginalPath(StorageRoot() / "other.conf"));
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
