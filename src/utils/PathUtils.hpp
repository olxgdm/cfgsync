#pragma once

#include <filesystem>

namespace cfgsync::utils {

std::filesystem::path ExpandUserPath(const std::filesystem::path& path);
std::filesystem::path NormalizePath(const std::filesystem::path& path);
std::filesystem::path MakeStorageRelativePath(const std::filesystem::path& originalPath);

}  // namespace cfgsync::utils
