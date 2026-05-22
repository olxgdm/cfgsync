#pragma once

#include <filesystem>
#include <string_view>

namespace cfgsync::utils {

enum class StoredRelativePathValidationError {
    None,
    Empty,
    Absolute,
    OutsideFiles,
    MissingFilesChild,
    ParentTraversal,
};

std::filesystem::path ExpandUserPath(const std::filesystem::path& path);
std::filesystem::path NormalizePath(const std::filesystem::path& path);
std::filesystem::path MakeStorageRelativePath(const std::filesystem::path& originalPath);
StoredRelativePathValidationError ValidateStoredRelativePath(std::string_view storedRelativePath);
std::string_view DescribeStoredRelativePathValidationError(StoredRelativePathValidationError error);

}  // namespace cfgsync::utils
