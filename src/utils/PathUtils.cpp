#include "utils/PathUtils.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

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
