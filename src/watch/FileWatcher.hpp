#pragma once

#include "watch/FileWatchObserver.hpp"

#include <filesystem>

namespace cfgsync::watch {

class FileWatcher {
public:
    virtual ~FileWatcher() = default;

    virtual void WatchDirectory(const std::filesystem::path& directory, FileWatchObserver& observer) = 0;
    virtual void Start() = 0;
};

}  // namespace cfgsync::watch
