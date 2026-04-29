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

void WriteJsonFile(const fs::path& path, const nlohmann::json& document) {
    if (path.has_parent_path()) {
        cfgsync::utils::EnsureDirectoryExists(path.parent_path());
    }

    std::ofstream output{path};
    output << document.dump(4) << '\n';
}

class RegistryInitTest : public testing::Test {
protected:
    void SetUp() override { TestRoot = cfgsync::tests::MakeTestRoot(); }

    void TearDown() override { fs::remove_all(TestRoot); }

    const fs::path& GetTestRoot() const { return TestRoot; }

private:
    fs::path TestRoot;
};

TEST_F(RegistryInitTest, InitializesMissingStorageDirectory) {
    const auto storageRoot = GetTestRoot() / "missing-storage";
    cfgsync::core::Registry registry;

    registry.Initialize(storageRoot);

    const auto normalizedStorageRoot = cfgsync::utils::NormalizePath(storageRoot);
    const auto registryPath = normalizedStorageRoot / "registry.json";

    EXPECT_TRUE(fs::is_directory(normalizedStorageRoot));
    EXPECT_TRUE(fs::is_directory(normalizedStorageRoot / "files"));
    EXPECT_TRUE(fs::is_regular_file(registryPath));

    const auto document = ReadJsonFile(registryPath);
    EXPECT_EQ(document["version"], 1);
    EXPECT_EQ(document["storage_root"], normalizedStorageRoot.string());
    EXPECT_TRUE(document["tracked_files"].is_array());
    EXPECT_TRUE(document["tracked_files"].empty());
}

TEST_F(RegistryInitTest, InitializesExistingEmptyStorageDirectory) {
    const auto storageRoot = GetTestRoot() / "existing-storage";
    cfgsync::utils::EnsureDirectoryExists(storageRoot);
    cfgsync::core::Registry registry;

    registry.Initialize(storageRoot);

    EXPECT_TRUE(fs::is_directory(storageRoot / "files"));
    EXPECT_TRUE(fs::is_regular_file(storageRoot / "registry.json"));
}

TEST_F(RegistryInitTest, RerunWithValidRegistryPreservesTrackedEntries) {
    const auto storageRoot = GetTestRoot() / "valid-storage";
    const auto normalizedStorageRoot = cfgsync::utils::NormalizePath(storageRoot);
    const auto registryPath = normalizedStorageRoot / "registry.json";

    const nlohmann::json existingRegistry = {
        {"version", 1},
        {"storage_root", normalizedStorageRoot.string()},
        {"tracked_files", nlohmann::json::array({
                              {
                                  {"original_path", normalizedStorageRoot.string() + "/source.conf"},
                                  {"stored_relative_path", "files/source.conf"},
                              },
                          })},
    };
    WriteJsonFile(registryPath, existingRegistry);

    cfgsync::core::Registry registry;
    registry.Initialize(storageRoot);

    const auto document = ReadJsonFile(registryPath);
    EXPECT_EQ(document, existingRegistry);
    ASSERT_EQ(registry.GetTrackedEntries().size(), 1U);
    EXPECT_EQ(registry.GetTrackedEntries()[0].OriginalPath, normalizedStorageRoot.string() + "/source.conf");
    EXPECT_EQ(registry.GetTrackedEntries()[0].StoredRelativePath, "files/source.conf");
}

TEST_F(RegistryInitTest, RerunWithValidRegistryRecreatesMissingFilesDirectory) {
    const auto storageRoot = GetTestRoot() / "missing-files-directory";
    const auto normalizedStorageRoot = cfgsync::utils::NormalizePath(storageRoot);
    const auto registryPath = normalizedStorageRoot / "registry.json";

    WriteJsonFile(registryPath, {
                                    {"version", 1},
                                    {"storage_root", normalizedStorageRoot.string()},
                                    {"tracked_files", nlohmann::json::array()},
                                });

    ASSERT_FALSE(fs::exists(normalizedStorageRoot / "files"));

    cfgsync::core::Registry registry;
    registry.Initialize(storageRoot);

    EXPECT_TRUE(fs::is_directory(normalizedStorageRoot / "files"));
}

TEST_F(RegistryInitTest, MalformedExistingRegistryThrowsClearError) {
    const auto storageRoot = GetTestRoot() / "malformed-storage";
    const auto registryPath = storageRoot / "registry.json";
    cfgsync::utils::EnsureDirectoryExists(storageRoot);

    std::ofstream output{registryPath};
    output << "{ invalid json";
    output.close();

    cfgsync::core::Registry registry;

    try {
        registry.Initialize(storageRoot);
        FAIL() << "Malformed registry did not throw.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Malformed cfgsync registry"), std::string::npos);
    }
}

TEST_F(RegistryInitTest, MissingTrackedFilesThrowsClearError) {
    const auto storageRoot = GetTestRoot() / "missing-tracked-files";
    const auto normalizedStorageRoot = cfgsync::utils::NormalizePath(storageRoot);

    WriteJsonFile(normalizedStorageRoot / "registry.json", {
                                                               {"version", 1},
                                                               {"storage_root", normalizedStorageRoot.string()},
                                                           });

    cfgsync::core::Registry registry;

    try {
        registry.Initialize(storageRoot);
        FAIL() << "Registry without tracked_files did not throw.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("tracked_files must be an array"), std::string::npos);
    }
}

TEST_F(RegistryInitTest, UnsupportedVersionThrowsClearError) {
    const auto storageRoot = GetTestRoot() / "unsupported-version";
    const auto normalizedStorageRoot = cfgsync::utils::NormalizePath(storageRoot);

    WriteJsonFile(normalizedStorageRoot / "registry.json", {
                                                               {"version", 999},
                                                               {"storage_root", normalizedStorageRoot.string()},
                                                               {"tracked_files", nlohmann::json::array()},
                                                           });

    cfgsync::core::Registry registry;

    try {
        registry.Initialize(storageRoot);
        FAIL() << "Unsupported registry version did not throw.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Unsupported cfgsync registry version"), std::string::npos);
    }
}

TEST_F(RegistryInitTest, MismatchedStorageRootThrowsClearError) {
    const auto storageRoot = GetTestRoot() / "actual-storage";
    const auto otherStorageRoot = cfgsync::utils::NormalizePath(GetTestRoot() / "other-storage");

    WriteJsonFile(storageRoot / "registry.json", {
                                                     {"version", 1},
                                                     {"storage_root", otherStorageRoot.string()},
                                                     {"tracked_files", nlohmann::json::array()},
                                                 });

    cfgsync::core::Registry registry;

    try {
        registry.Initialize(storageRoot);
        FAIL() << "Mismatched registry storage root did not throw.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("belongs to storage root"), std::string::npos);
    }
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
