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
#include <string_view>
#include <vector>

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

fs::path ExpectedStorageRelativePathForNormalizedPath(const fs::path& normalizedPath) {
    fs::path expectedPath{"files"};
    const auto rootName = normalizedPath.root_name().generic_string();
    if (!rootName.empty()) {
        std::string sanitizedRoot = rootName;
        sanitizedRoot.erase(
            std::remove_if(sanitizedRoot.begin(), sanitizedRoot.end(),
                           [](char character) { return character == ':' || character == '/' || character == '\\'; }),
            sanitizedRoot.end());
        if (!sanitizedRoot.empty()) {
            expectedPath /= sanitizedRoot;
        }
    }

    for (const auto& component : normalizedPath) {
        if (!normalizedPath.root_name().empty() && component == normalizedPath.root_name()) {
            continue;
        }

        if (component == normalizedPath.root_directory()) {
            continue;
        }

        expectedPath /= component;
    }

    return expectedPath;
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

TEST_F(PathFileUtilsTest, NormalizePathExpandsBareUserHomeWhenAvailable) {
    const auto home = GetTestRoot() / "home";
    cfgsync::utils::EnsureDirectoryExists(home);

#ifdef _WIN32
    SetEnvironmentVariable("USERPROFILE", home.string());
#else
    SetEnvironmentVariable("HOME", home.string());
#endif

    const auto normalizedPath = cfgsync::utils::NormalizePath("~");

    EXPECT_EQ(normalizedPath, home.lexically_normal());
}

TEST_F(PathFileUtilsTest, NormalizePathLeavesTildeUserPathUnexpanded) {
    const auto normalizedPath = cfgsync::utils::NormalizePath("~other/config");

    EXPECT_TRUE(normalizedPath.is_absolute());
    EXPECT_NE(normalizedPath.string().find("~other"), std::string::npos);
}

TEST_F(PathFileUtilsTest, NormalizePathLeavesHomePathUnexpandedWhenHomeIsUnavailable) {
#ifndef _WIN32
    const auto previousHome = GetEnvironmentVariable("HOME");
    UnsetEnvironmentVariable("HOME");

    const auto normalizedPath = cfgsync::utils::NormalizePath("~/config/file.conf");

    EXPECT_TRUE(normalizedPath.is_absolute());
    EXPECT_NE(normalizedPath.string().find("~"), std::string::npos);

    RestoreEnvironmentVariable("HOME", previousHome);
#else
    GTEST_SKIP() << "Windows home fallback uses multiple environment variables.";
#endif
}

TEST_F(PathFileUtilsTest, MakeStorageRelativePathMapsPosixPathUnderFiles) {
    const auto storagePath = cfgsync::utils::MakeStorageRelativePath("/home/user/.gitconfig");

    EXPECT_EQ(storagePath.generic_string(), "files/home/user/.gitconfig");
}

TEST_F(PathFileUtilsTest, MakeStorageRelativePathNormalizesPosixPathBeforeMapping) {
    const auto storagePath = cfgsync::utils::MakeStorageRelativePath("/home/user/../user/.config/./nvim/init.lua");

    EXPECT_EQ(storagePath.generic_string(), "files/home/user/.config/nvim/init.lua");
}

TEST_F(PathFileUtilsTest, MakeStorageRelativePathMapsNestedPosixPathUnderFiles) {
    const auto storagePath = cfgsync::utils::MakeStorageRelativePath("/home/user/.config/nvim/init.lua");

    EXPECT_EQ(storagePath.generic_string(), "files/home/user/.config/nvim/init.lua");
}

TEST_F(PathFileUtilsTest, MakeStorageRelativePathMapsWindowsDrivePathUnderFiles) {
    const auto storagePath = cfgsync::utils::MakeStorageRelativePath(R"(C:\Users\Oleksii\.gitconfig)");

    EXPECT_EQ(storagePath.generic_string(), "files/C/Users/Oleksii/.gitconfig");
}

TEST_F(PathFileUtilsTest, MakeStorageRelativePathMapsWindowsDrivePathsWithSlashVariantsUnderFiles) {
    struct TestCase {
        std::string_view Input;
        std::string_view Expected;
    };

    const std::vector<TestCase> testCases{
        {R"(C:/Users/Oleksii/.gitconfig)", "files/C/Users/Oleksii/.gitconfig"},
        {R"(C:\Users\Oleksii\.gitconfig)", "files/C/Users/Oleksii/.gitconfig"},
        {R"(C:\Users/Oleksii\.config/nvim/init.lua)", "files/C/Users/Oleksii/.config/nvim/init.lua"},
    };

    for (const auto& testCase : testCases) {
        SCOPED_TRACE(testCase.Input);
        const auto storagePath = cfgsync::utils::MakeStorageRelativePath(fs::path{testCase.Input});

        EXPECT_EQ(storagePath.generic_string(), testCase.Expected);
    }
}

TEST_F(PathFileUtilsTest, MakeStorageRelativePathNormalizesRelativeInputUnderFiles) {
    const auto inputPath = fs::path{"relative"} / ".." / "cfgsync-relative.conf";
    const auto normalizedPath = cfgsync::utils::NormalizePath(inputPath);

    const auto storagePath = cfgsync::utils::MakeStorageRelativePath(inputPath);

    EXPECT_EQ(storagePath, ExpectedStorageRelativePathForNormalizedPath(normalizedPath));
    EXPECT_EQ(cfgsync::utils::ValidateStoredRelativePath(storagePath.generic_string()),
              cfgsync::utils::StoredRelativePathValidationError::None);
}

TEST_F(PathFileUtilsTest, ValidateStoredRelativePathAcceptsFilesChildrenWithSlashVariants) {
    const std::vector<std::string> validStoredRelativePaths{
        "files/home/user/.gitconfig",
        R"(files\home\user\.gitconfig)",
        R"(files/home\user/.config\nvim/init.lua)",
    };

    for (const auto& storedRelativePath : validStoredRelativePaths) {
        SCOPED_TRACE(storedRelativePath);
        EXPECT_EQ(cfgsync::utils::ValidateStoredRelativePath(storedRelativePath),
                  cfgsync::utils::StoredRelativePathValidationError::None);
    }
}

TEST_F(PathFileUtilsTest, ValidateStoredRelativePathRejectsMalformedOrRootedPaths) {
    struct TestCase {
        std::string StoredRelativePath;
        cfgsync::utils::StoredRelativePathValidationError ExpectedError;
    };

    const std::vector<TestCase> testCases{
        {"", cfgsync::utils::StoredRelativePathValidationError::Empty},
        {"/files/x", cfgsync::utils::StoredRelativePathValidationError::Absolute},
        {"C:files/x", cfgsync::utils::StoredRelativePathValidationError::Absolute},
        {R"(C:\files\x)", cfgsync::utils::StoredRelativePathValidationError::Absolute},
        {"files/../x", cfgsync::utils::StoredRelativePathValidationError::ParentTraversal},
        {R"(files\..\x)", cfgsync::utils::StoredRelativePathValidationError::ParentTraversal},
        {"backup/foo", cfgsync::utils::StoredRelativePathValidationError::OutsideFiles},
        {"files", cfgsync::utils::StoredRelativePathValidationError::MissingFilesChild},
    };

    for (const auto& testCase : testCases) {
        SCOPED_TRACE(testCase.StoredRelativePath);
        EXPECT_EQ(cfgsync::utils::ValidateStoredRelativePath(testCase.StoredRelativePath), testCase.ExpectedError);
    }
}

TEST_F(PathFileUtilsTest, GeneratedStorageRelativePathsAlwaysValidate) {
    const auto home = GetTestRoot() / "home";
    cfgsync::utils::EnsureDirectoryExists(home);

#ifdef _WIN32
    const auto previousUserProfile = GetEnvironmentVariable("USERPROFILE");
    SetEnvironmentVariable("USERPROFILE", home.string());
#else
    const auto previousHome = GetEnvironmentVariable("HOME");
    SetEnvironmentVariable("HOME", home.string());
#endif

    const std::vector<fs::path> originalPaths{
        "/home/user/.gitconfig",
        R"(C:\Users\Oleksii\.gitconfig)",
        R"(C:\Users/Oleksii\.config/nvim/init.lua)",
        fs::path{"relative"} / ".." / "cfgsync-relative.conf",
        "~/config/file.conf",
    };

    for (const auto& originalPath : originalPaths) {
        SCOPED_TRACE(originalPath.generic_string());
        const auto storedRelativePath = cfgsync::utils::MakeStorageRelativePath(originalPath).generic_string();

        EXPECT_EQ(cfgsync::utils::ValidateStoredRelativePath(storedRelativePath),
                  cfgsync::utils::StoredRelativePathValidationError::None);
    }

#ifdef _WIN32
    RestoreEnvironmentVariable("USERPROFILE", previousUserProfile);
#else
    RestoreEnvironmentVariable("HOME", previousHome);
#endif
}

TEST_F(PathFileUtilsTest, OrdinaryFileValidationAcceptsRegularFile) {
    const auto filePath = GetTestRoot() / "source.conf";
    WriteTextFile(filePath, "value=true\n");

    EXPECT_TRUE(cfgsync::utils::IsOrdinaryFile(filePath));
    EXPECT_NO_THROW(cfgsync::utils::RequireOrdinaryFile(filePath));
}

TEST_F(PathFileUtilsTest, EmptyPathValidationFailsClearly) {
    EXPECT_FALSE(cfgsync::utils::IsOrdinaryFile({}));
    EXPECT_THROW(cfgsync::utils::RequireOrdinaryFile({}), std::invalid_argument);
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

TEST_F(PathFileUtilsTest, EnsureDirectoryExistsAllowsEmptyPath) {
    EXPECT_NO_THROW(cfgsync::utils::EnsureDirectoryExists({}));
}

TEST_F(PathFileUtilsTest, CopyFileRejectsEmptySourceAndDestination) {
    const auto sourcePath = GetTestRoot() / "source.conf";
    const auto destinationPath = GetTestRoot() / "destination.conf";
    WriteTextFile(sourcePath, "value=true\n");

    EXPECT_THROW(cfgsync::utils::CopyFile({}, destinationPath), std::invalid_argument);
    EXPECT_THROW(cfgsync::utils::CopyFile(sourcePath, {}), std::invalid_argument);
}

TEST_F(PathFileUtilsTest, CopyFileFailureReportsContext) {
    const auto missingSourcePath = GetTestRoot() / "missing.conf";
    const auto destinationPath = GetTestRoot() / "destination.conf";

    try {
        cfgsync::utils::CopyFile(missingSourcePath, destinationPath);
        FAIL() << "Copying a missing source did not throw.";
    } catch (const cfgsync::FileError& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find(missingSourcePath.string()), std::string::npos);
        EXPECT_NE(message.find(destinationPath.string()), std::string::npos);
    }
}

}  // namespace

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
