#pragma once

#include "core/Registry.hpp"
#include "gtest/gtest.h"
#include "utils/PathUtils.hpp"

#include <filesystem>

namespace cfgsync::tests {

inline std::filesystem::path TrackFile(core::Registry& registry, const std::filesystem::path& sourcePath) {
    const auto normalizedSourcePath = utils::NormalizePath(sourcePath);
    const auto storedRelativePath = utils::MakeStorageRelativePath(normalizedSourcePath);
    const auto added = registry.AddEntry({
        .OriginalPath = normalizedSourcePath.string(),
        .StoredRelativePath = storedRelativePath.generic_string(),
    });
    EXPECT_TRUE(added);
    registry.Save();
    return storedRelativePath;
}

}  // namespace cfgsync::tests
