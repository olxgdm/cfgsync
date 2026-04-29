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

std::vector<std::string> SplitNormalizedWindowsComponents(std::string_view input) {
    std::string normalizedInput{input};
    std::replace(normalizedInput.begin(), normalizedInput.end(), '\\', '/');

    std::vector<std::string> components;
    std::string currentComponent;
    for (std::size_t index = 3; index < normalizedInput.size(); ++index) {
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

fs::path MakeWindowsDriveStorageRelativePath(std::string_view input) {
    fs::path storageRelativePath{"files"};
    const auto drive = static_cast<char>(std::toupper(static_cast<unsigned char>(input[0])));
    storageRelativePath /= std::string{drive};

    for (const auto& component : SplitNormalizedWindowsComponents(input)) {
        storageRelativePath /= component;
    }

    return storageRelativePath;
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
    const auto originalPathText = originalPath.string();
    if (IsWindowsDriveAbsolutePath(originalPathText)) {
        return MakeWindowsDriveStorageRelativePath(originalPathText);
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

}  // namespace cfgsync::utils
