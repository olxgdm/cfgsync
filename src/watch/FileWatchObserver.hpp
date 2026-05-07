#pragma once

#include "watch/FileWatchEvent.hpp"

namespace cfgsync::watch {

class FileWatchObserver {
public:
    virtual ~FileWatchObserver() = default;

    virtual void OnFileChanged(const FileWatchEvent& event) = 0;
};

}  // namespace cfgsync::watch
