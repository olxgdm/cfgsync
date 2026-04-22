#pragma once

#include <string>

namespace cfgsync::core {

struct TrackedEntry {
    std::string OriginalPath;
    std::string StoredRelativePath;
};

}  // namespace cfgsync::core
