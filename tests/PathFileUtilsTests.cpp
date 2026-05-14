#include "Exceptions.hpp"
#include "common/TestTempDirectory.hpp"
#include "gtest/gtest.h"
#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>

#ifdef _WIN32
#include <cstdlib>
#else
#include <cstdlib>
#endif

namespace {
namespace fs = std::filesystem;

void WriteTextFile(const fs::path& path, const std::string& contents) {
    if (path.has_parent_path()) {
        cfgsync::utils::EnsureDirectoryExists(path.parent_path());
    }

    std::ofstream output{path};
    output << contents;
}

std::string ReadTextFile(const fs::path& path) {
    std::ifstream input{path};
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void SetEnvironmentVariable(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void UnsetEnvironmentVariable(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

std::optional<std::string> GetEnvironmentVariable(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }

    return std::string{value};
}

void RestoreEnvironmentVariable(const char* name, const std::optional<std::string>& value) {
    if (value.has_value()) {
        SetEnvironmentVariable(name, value.value());
        return;
    }

    UnsetEnvironmentVariable(name);
}

class PathFileUtilsTest : public testing::Test {
protected:
    void SetUp() override { TestRoot = cfgsync::tests::MakeTestRoot(); }

    void TearDown() override { fs::remove_all(TestRoot); }

    fs::path GetTestRoot() const { return TestRoot; }

private:
    fs::path TestRoot;
};

TEST_F(PathFileUtilsTest, NormalizePathReturnsEmptyForEmptyInput) {
    EXPECT_TRUE(cfgsync::utils::NormalizePath({}).empty());
}

TEST_F(PathFileUtilsTest, NormalizePathConvertsRelativeInputToAbsoluteNormalizedPath) {
    const auto path = fs::path{"."} / "src" / ".." / "README.md";

    const auto normalizedPath = cfgsync::utils::NormalizePath(path);

    EXPECT_TRUE(normalizedPath.is_absolute());
    EXPECT_EQ(normalizedPath, fs::absolute("README.md").lexically_normal());
}

TEST_F(PathFileUtilsTest, NormalizePathExpandsUserHomeWhenAvailable) {
    const auto home = GetTestRoot() / "home";
    cfgsync::utils::EnsureDirectoryExists(home);

#ifdef _WIN32
    SetEnvironmentVariable("USERPROFILE", home.string());
#else
    SetEnvironmentVariable("HOME", home.string());
#endif

    const auto normalizedPath = cfgsync::utils::NormalizePath("~/config/file.conf");

    EXPECT_EQ(normalizedPath, (home / "config" / "file.conf").lexically_normal());
}

TEST_F(PathFileUtilsTest, MakeStorageRelativePathMapsPosixPathUnderFiles) {
    const auto storagePath = cfgsync::utils::MakeStorageRelativePath("/home/user/.gitconfig");

    EXPECT_EQ(storagePath.generic_string(), "files/home/user/.gitconfig");
}

TEST_F(PathFileUtilsTest, MakeStorageRelativePathMapsNestedPosixPathUnderFiles) {
    const auto storagePath = cfgsync::utils::MakeStorageRelativePath("/home/user/.config/nvim/init.lua");

    EXPECT_EQ(storagePath.generic_string(), "files/home/user/.config/nvim/init.lua");
}

TEST_F(PathFileUtilsTest, MakeStorageRelativePathMapsWindowsDrivePathUnderFiles) {
    const auto storagePath = cfgsync::utils::MakeStorageRelativePath(R"(C:\Users\Oleksii\.gitconfig)");

    EXPECT_EQ(storagePath.generic_string(), "files/C/Users/Oleksii/.gitconfig");
}

TEST_F(PathFileUtilsTest, OrdinaryFileValidationAcceptsRegularFile) {
    const auto filePath = GetTestRoot() / "source.conf";
    WriteTextFile(filePath, "value=true\n");

    EXPECT_TRUE(cfgsync::utils::IsOrdinaryFile(filePath));
    EXPECT_NO_THROW(cfgsync::utils::RequireOrdinaryFile(filePath));
}

TEST_F(PathFileUtilsTest, OrdinaryFileValidationRejectsMissingPath) {
    const auto filePath = GetTestRoot() / "missing.conf";

    EXPECT_FALSE(cfgsync::utils::IsOrdinaryFile(filePath));
    EXPECT_THROW(cfgsync::utils::RequireOrdinaryFile(filePath), cfgsync::FileError);
}

TEST_F(PathFileUtilsTest, OrdinaryFileValidationRejectsDirectory) {
    const auto directoryPath = GetTestRoot() / "configs";
    cfgsync::utils::EnsureDirectoryExists(directoryPath);

    EXPECT_FALSE(cfgsync::utils::IsOrdinaryFile(directoryPath));
    EXPECT_THROW(cfgsync::utils::RequireOrdinaryFile(directoryPath), cfgsync::FileError);
}

TEST_F(PathFileUtilsTest, OrdinaryFileValidationRejectsSymlink) {
    const auto targetPath = GetTestRoot() / "target.conf";
    const auto symlinkPath = GetTestRoot() / "linked.conf";
    WriteTextFile(targetPath, "value=true\n");

    std::error_code errorCode;
    fs::create_symlink(targetPath, symlinkPath, errorCode);
    if (errorCode) {
        GTEST_SKIP() << "Symlink creation is not available in this test environment.";
    }

    EXPECT_FALSE(cfgsync::utils::IsOrdinaryFile(symlinkPath));
    EXPECT_THROW(cfgsync::utils::RequireOrdinaryFile(symlinkPath), cfgsync::FileError);
}

TEST_F(PathFileUtilsTest, CopyFileCreatesDestinationParentDirectories) {
    const auto sourcePath = GetTestRoot() / "source.conf";
    const auto destinationPath = GetTestRoot() / "nested" / "configs" / "source.conf";
    WriteTextFile(sourcePath, "value=true\n");

    cfgsync::utils::CopyFile(sourcePath, destinationPath);

    EXPECT_TRUE(fs::exists(destinationPath));
    EXPECT_EQ(ReadTextFile(destinationPath), "value=true\n");
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
