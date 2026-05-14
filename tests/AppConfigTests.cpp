#include "Exceptions.hpp"
#include "common/TestTempDirectory.hpp"
#include "core/AppConfig.hpp"
#include "gtest/gtest.h"
#include "utils/AppConfigPath.hpp"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
namespace fs = std::filesystem;

void SetEnvironmentVariable(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

class AppConfigTest : public testing::Test {
protected:
    void SetUp() override {
        TestRoot = cfgsync::tests::MakeTestRoot();
        cfgsync::utils::EnsureDirectoryExists(TestRoot);
    }

    void TearDown() override { fs::remove_all(TestRoot); }

    const fs::path& GetTestRoot() { return TestRoot; }

private:
    fs::path TestRoot;
};

TEST_F(AppConfigTest, SavesAndLoadsStorageRoot) {
    const auto configPath = GetTestRoot() / "config" / "config.json";
    const auto storageRoot = GetTestRoot() / "storage";

    cfgsync::core::AppConfig saved{configPath};
    saved.SetStorageRoot(storageRoot);
    saved.Save();

    cfgsync::core::AppConfig loaded{configPath};
    loaded.Load();

    EXPECT_EQ(loaded.GetStorageRoot(), cfgsync::utils::NormalizePath(storageRoot));
}

TEST_F(AppConfigTest, MissingConfigThrowsActionableError) {
    cfgsync::core::AppConfig config{GetTestRoot() / "missing" / "config.json"};

    try {
        config.Load();
        FAIL() << "Missing app config did not throw.";
    } catch (const cfgsync::ConfigError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("cfgsync has not been initialized"), std::string::npos);
        EXPECT_NE(message.find("cfgsync init --storage <dir>"), std::string::npos);
    }
}

TEST_F(AppConfigTest, MalformedConfigThrowsClearError) {
    const auto configPath = GetTestRoot() / "malformed" / "config.json";
    cfgsync::utils::EnsureDirectoryExists(configPath.parent_path());

    std::ofstream output{configPath};
    output << "{ invalid json";
    output.close();

    cfgsync::core::AppConfig config{configPath};
    try {
        config.Load();
        FAIL() << "Malformed app config did not throw.";
    } catch (const cfgsync::ConfigError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Malformed cfgsync app config"), std::string::npos);
    }
}

TEST_F(AppConfigTest, NonIntegerVersionThrowsClearError) {
    const auto configPath = GetTestRoot() / "invalid-version" / "config.json";
    cfgsync::utils::EnsureDirectoryExists(configPath.parent_path());

    std::ofstream output{configPath};
    output << R"({
        "version": "1",
        "storage_root": "/tmp/cfgsync-storage"
    })";
    output.close();

    cfgsync::core::AppConfig config{configPath};
    try {
        config.Load();
        FAIL() << "Invalid app config version did not throw.";
    } catch (const cfgsync::ConfigError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("version must be an integer"), std::string::npos);
    }
}

TEST_F(AppConfigTest, NonObjectConfigThrowsClearError) {
    const auto configPath = GetTestRoot() / "non-object" / "config.json";
    cfgsync::utils::EnsureDirectoryExists(configPath.parent_path());

    std::ofstream output{configPath};
    output << R"(["not", "an", "object"])";
    output.close();

    cfgsync::core::AppConfig config{configPath};

    try {
        config.Load();
        FAIL() << "Non-object app config did not throw.";
    } catch (const cfgsync::ConfigError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("root value must be an object"), std::string::npos);
    }
}

TEST_F(AppConfigTest, UnsupportedVersionThrowsClearError) {
    const auto configPath = GetTestRoot() / "unsupported-version" / "config.json";
    cfgsync::utils::EnsureDirectoryExists(configPath.parent_path());

    std::ofstream output{configPath};
    output << R"({
        "version": 99,
        "storage_root": "/tmp/cfgsync-storage"
    })";
    output.close();

    cfgsync::core::AppConfig config{configPath};

    try {
        config.Load();
        FAIL() << "Unsupported app config version did not throw.";
    } catch (const cfgsync::ConfigError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("Unsupported cfgsync app config version 99"), std::string::npos);
    }
}

TEST_F(AppConfigTest, MissingStorageRootThrowsClearError) {
    const auto configPath = GetTestRoot() / "missing-storage-root" / "config.json";
    cfgsync::utils::EnsureDirectoryExists(configPath.parent_path());

    std::ofstream output{configPath};
    output << R"({
        "version": 1
    })";
    output.close();

    cfgsync::core::AppConfig config{configPath};

    try {
        config.Load();
        FAIL() << "Missing storage_root did not throw.";
    } catch (const cfgsync::ConfigError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("storage_root must be a string"), std::string::npos);
    }
}

TEST_F(AppConfigTest, EmptyStorageRootThrowsClearError) {
    const auto configPath = GetTestRoot() / "empty-storage-root" / "config.json";
    cfgsync::utils::EnsureDirectoryExists(configPath.parent_path());

    std::ofstream output{configPath};
    output << R"({
        "version": 1,
        "storage_root": ""
    })";
    output.close();

    cfgsync::core::AppConfig config{configPath};

    try {
        config.Load();
        FAIL() << "Empty storage_root did not throw.";
    } catch (const cfgsync::ConfigError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("storage_root must not be empty"), std::string::npos);
    }
}

TEST_F(AppConfigTest, EmptyConfigPathFailsLoadAndSaveClearly) {
    cfgsync::core::AppConfig config;

    EXPECT_THROW(config.Load(), cfgsync::ConfigError);

    config.SetStorageRoot(GetTestRoot() / "storage");
    EXPECT_THROW(config.Save(), cfgsync::ConfigError);
}

TEST_F(AppConfigTest, SaveWithoutStorageRootFailsClearly) {
    cfgsync::core::AppConfig config{GetTestRoot() / "config.json"};

    EXPECT_THROW(config.Save(), cfgsync::ConfigError);
}

TEST_F(AppConfigTest, ResolvesDefaultConfigPathFromPlatformEnvironment) {
#ifdef _WIN32
    SetEnvironmentVariable("APPDATA", GetTestRoot().string());
    const auto expectedPath = GetTestRoot() / "cfgsync" / "config.json";
#else
    SetEnvironmentVariable("HOME", GetTestRoot().string());
    const auto expectedPath = GetTestRoot() / ".config" / "cfgsync" / "config.json";
#endif

    EXPECT_EQ(cfgsync::utils::GetDefaultAppConfigPath(), expectedPath);
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
