#pragma once

#include "utils/FileUtils.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <string>

namespace cfgsync::tests {

inline void WriteTextFile(const std::filesystem::path& path, const std::string& contents) {
    if (path.has_parent_path()) {
        utils::EnsureDirectoryExists(path.parent_path());
    }

    std::ofstream output{path};
    output << contents;
}

inline std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream input{path};
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

inline nlohmann::json ReadJsonFile(const std::filesystem::path& path) {
    std::ifstream input{path};
    nlohmann::json document;
    input >> document;
    return document;
}

inline std::filesystem::path StoredPathFor(const std::filesystem::path& storageRoot,
                                           const std::filesystem::path& sourcePath) {
    const auto normalizedSourcePath = utils::NormalizePath(sourcePath);
    return storageRoot / utils::MakeStorageRelativePath(normalizedSourcePath);
}

inline std::filesystem::path StoredRegistryPathFor(const std::filesystem::path& storageRoot,
                                                   const std::filesystem::path& sourcePath) {
    const auto normalizedSourcePath = utils::NormalizePath(sourcePath);
    return storageRoot / utils::MakeStorageRelativePath(normalizedSourcePath).generic_string();
}

}  // namespace cfgsync::tests
