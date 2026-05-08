#pragma once

#include "watch/FileWatcher.hpp"

#include <filesystem>
#include <memory>

namespace cfgsync::watch {

class EfswFileWatcher final : public FileWatcher {
public:
    EfswFileWatcher();
    ~EfswFileWatcher() noexcept override;

    EfswFileWatcher(const EfswFileWatcher&) = delete;
    EfswFileWatcher& operator=(const EfswFileWatcher&) = delete;
    EfswFileWatcher(EfswFileWatcher&&) noexcept;
    EfswFileWatcher& operator=(EfswFileWatcher&&) noexcept;

    void WatchDirectory(const std::filesystem::path& directory, FileWatchObserver& observer) override;
    void Start() override;

private:
    class Impl;
    std::unique_ptr<Impl> Impl_ = nullptr;
};

}  // namespace cfgsync::watch
