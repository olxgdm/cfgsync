#pragma once

#include <filesystem>

namespace cfgsync::core {

class AppConfig {
public:
    explicit AppConfig(std::filesystem::path configPath = {});

    void SetConfigPath(std::filesystem::path configPath);
    const std::filesystem::path& GetConfigPath() const;

    void SetStorageRoot(const std::filesystem::path& storageRoot);
    const std::filesystem::path& GetStorageRoot() const;

    void Load();
    void Save() const;

private:
    std::filesystem::path ConfigPath_;
    std::filesystem::path StorageRoot_;
};

}  // namespace cfgsync::core
