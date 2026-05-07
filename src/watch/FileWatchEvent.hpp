#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace cfgsync::watch {

enum class FileWatchAction : std::uint8_t {
    Added,
    Deleted,
    Modified,
    Moved,
};

struct FileWatchEvent {
    FileWatchAction Action;
    std::filesystem::path Directory;
    std::filesystem::path Path;
    std::optional<std::filesystem::path> OldPath;
};

}  // namespace cfgsync::watch
