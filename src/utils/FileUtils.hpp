#pragma once

#include <filesystem>

namespace cfgsync::utils {

void EnsureDirectoryExists(const std::filesystem::path& path);
void CopyFile(const std::filesystem::path& source, const std::filesystem::path& destination);

}  // namespace cfgsync::utils
