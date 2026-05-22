#include "utils/PathUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace cfgsync::utils {
namespace fs = std::filesystem;

namespace {

std::string GetHomeDirectory() {
#ifdef _WIN32
    if (const char* userProfile = std::getenv("USERPROFILE")) {
        return userProfile;
    }

    const char* homeDrive = std::getenv("HOMEDRIVE");
    const char* homePath = std::getenv("HOMEPATH");
    if (homeDrive != nullptr && homePath != nullptr) {
        return std::string{homeDrive} + homePath;
    }
#else
    if (const char* home = std::getenv("HOME")) {
        return home;
    }
#endif

    return {};
}

bool IsPathSeparator(char character) { return character == '/' || character == '\\'; }

bool IsWindowsDriveAbsolutePath(std::string_view input) {
    return input.size() >= 3 && std::isalpha(static_cast<unsigned char>(input[0])) != 0 && input[1] == ':' &&
           IsPathSeparator(input[2]);
}

bool IsStoredPathRooted(std::string_view storedRelativePath) {
    if (storedRelativePath.empty()) {
        return false;
    }

    if (IsPathSeparator(storedRelativePath[0])) {
        return true;
    }

    return storedRelativePath.size() >= 2 &&
           std::isalpha(static_cast<unsigned char>(storedRelativePath[0])) != 0 && storedRelativePath[1] == ':';
}

bool IsPosixAbsolutePath(std::string_view input) { return input.starts_with('/') && !input.starts_with("//"); }

std::vector<std::string> SplitPathComponents(std::string_view input) {
    std::vector<std::string> components;
    std::string currentComponent;
    for (const auto character : input) {
        if (!IsPathSeparator(character)) {
            currentComponent.push_back(character);
            continue;
        }

        if (!currentComponent.empty()) {
            components.push_back(currentComponent);
            currentComponent.clear();
        }
    }

    if (!currentComponent.empty()) {
        components.push_back(currentComponent);
    }

    return components;
}

std::vector<std::string> SplitNormalizedComponents(std::string_view input, std::size_t startIndex) {
    std::string normalizedInput{input};
    std::replace(normalizedInput.begin(), normalizedInput.end(), '\\', '/');

    std::vector<std::string> components;
    std::string currentComponent;
    for (std::size_t index = startIndex; index < normalizedInput.size(); ++index) {
        if (normalizedInput[index] != '/') {
            currentComponent.push_back(normalizedInput[index]);
            continue;
        }

        if (currentComponent.empty() || currentComponent == ".") {
            currentComponent.clear();
            continue;
        }

        if (currentComponent == "..") {
            if (!components.empty()) {
                components.pop_back();
            }
        } else {
            components.push_back(currentComponent);
        }
        currentComponent.clear();
    }

    if (!currentComponent.empty() && currentComponent != ".") {
        if (currentComponent == "..") {
            if (!components.empty()) {
                components.pop_back();
            }
        } else {
            components.push_back(currentComponent);
        }
    }

    return components;
}

fs::path BuildStorageRelativePath(std::string_view rootSegment, const std::vector<std::string>& components) {
    fs::path storageRelativePath{"files"};
    if (!rootSegment.empty()) {
        storageRelativePath /= rootSegment;
    }

    for (const auto& component : components) {
        storageRelativePath /= component;
    }

    return storageRelativePath;
}

fs::path MakeWindowsDriveStorageRelativePath(std::string_view input) {
    const auto drive = static_cast<char>(std::toupper(static_cast<unsigned char>(input[0])));
    const std::string driveSegment{drive};
    return BuildStorageRelativePath(driveSegment, SplitNormalizedComponents(input, 3));
}

fs::path MakePosixStorageRelativePath(std::string_view input) {
    return BuildStorageRelativePath({}, SplitNormalizedComponents(input, 1));
}

}  // namespace

fs::path ExpandUserPath(const fs::path& path) {
    const auto input = path.generic_string();
    if (input.empty() || input[0] != '~') {
        return path;
    }

    const auto homeDirectory = GetHomeDirectory();
    if (homeDirectory.empty()) {
        return path;
    }

    if (input == "~") {
        return fs::path{homeDirectory};
    }

    if (input.starts_with("~/")) {
        return fs::path{homeDirectory} / input.substr(2);
    }

    return path;
}

fs::path NormalizePath(const fs::path& path) {
    if (path.empty()) {
        return {};
    }

    const auto expandedPath = ExpandUserPath(path);
    const auto absolutePath = expandedPath.is_absolute() ? expandedPath : fs::absolute(expandedPath);
    return absolutePath.lexically_normal();
}

fs::path MakeStorageRelativePath(const fs::path& originalPath) {
    const auto originalPathText = originalPath.generic_string();
    if (IsWindowsDriveAbsolutePath(originalPathText)) {
        return MakeWindowsDriveStorageRelativePath(originalPathText);
    }

    if (IsPosixAbsolutePath(originalPathText)) {
        return MakePosixStorageRelativePath(originalPathText);
    }

    const auto normalizedPath = NormalizePath(originalPath);
    if (normalizedPath.empty()) {
        return {};
    }

    fs::path storageRelativePath{"files"};

    const auto rootName = normalizedPath.root_name().generic_string();
    if (!rootName.empty()) {
        std::string sanitizedRoot = rootName;
        sanitizedRoot.erase(
            std::remove_if(sanitizedRoot.begin(), sanitizedRoot.end(),
                           [](char character) { return character == ':' || character == '/' || character == '\\'; }),
            sanitizedRoot.end());
        if (!sanitizedRoot.empty()) {
            storageRelativePath /= sanitizedRoot;
        }
    }

    for (const auto& component : normalizedPath) {
        if (!normalizedPath.root_name().empty() && component == normalizedPath.root_name()) {
            continue;
        }
        if (component == normalizedPath.root_directory()) {
            continue;
        }

        storageRelativePath /= component;
    }

    return storageRelativePath;
}

StoredRelativePathValidationError ValidateStoredRelativePath(std::string_view storedRelativePath) {
    if (storedRelativePath.empty()) {
        return StoredRelativePathValidationError::Empty;
    }

    if (fs::path{storedRelativePath}.is_absolute() || IsStoredPathRooted(storedRelativePath)) {
        return StoredRelativePathValidationError::Absolute;
    }

    const auto components = SplitPathComponents(storedRelativePath);
    if (components.empty() || components.front() != "files") {
        return StoredRelativePathValidationError::OutsideFiles;
    }

    if (components.size() < 2) {
        return StoredRelativePathValidationError::MissingFilesChild;
    }

    if (std::any_of(components.begin(), components.end(),
                    [](const std::string& component) { return component == ".."; })) {
        return StoredRelativePathValidationError::ParentTraversal;
    }

    return StoredRelativePathValidationError::None;
}

std::string_view DescribeStoredRelativePathValidationError(StoredRelativePathValidationError error) {
    switch (error) {
        case StoredRelativePathValidationError::None:
            return "is valid.";
        case StoredRelativePathValidationError::Empty:
            return "must not be empty.";
        case StoredRelativePathValidationError::Absolute:
            return "must be relative.";
        case StoredRelativePathValidationError::OutsideFiles:
            return "must be under files/.";
        case StoredRelativePathValidationError::MissingFilesChild:
            return "must include a path under files/.";
        case StoredRelativePathValidationError::ParentTraversal:
            return "must not contain parent directory traversal.";
    }

    return "is invalid.";
}

}  // namespace cfgsync::utils
