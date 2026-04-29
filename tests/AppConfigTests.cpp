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

fs::path MakeTestRoot() {
    const auto base = fs::temp_directory_path();  // NOSONAR: Safe for tests; path is not trusted, directory is created
                                                  // atomically and permissions are restricted.

    const auto pid =
#ifdef _WIN32
        static_cast<unsigned long>(_getpid());
#else
        static_cast<unsigned long>(getpid());
#endif

    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();

        const auto candidate = base / ("cfgsync-app-config-tests-" + std::to_string(pid) + "-" + std::to_string(now) +
                                       "-" + std::to_string(attempt));

        std::error_code ec;
        if (!fs::create_directory(candidate, ec)) {
            continue;
        }

        fs::permissions(candidate, fs::perms::owner_all, fs::perm_options::replace, ec);

        if (ec) {
            fs::remove_all(candidate);
            continue;
        }

        return candidate;
    }

    throw std::runtime_error("Failed to create private temporary test directory");
}

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
        TestRoot = MakeTestRoot();
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
    } catch (const std::runtime_error& error) {
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
    } catch (const std::runtime_error& error) {
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
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("version must be an integer"), std::string::npos);
    }
}

TEST_F(AppConfigTest, ResolvesDefaultConfigPathFromPlatformEnvironment) {
#ifdef _WIN32
    SetEnvironmentVariable("APPDATA", TestRoot.string());
    const auto expectedPath = TestRoot / "cfgsync" / "config.json";
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
