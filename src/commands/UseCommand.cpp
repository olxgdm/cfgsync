#include "commands/UseCommand.hpp"

#include "Exceptions.hpp"
#include "utils/FileUtils.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <filesystem>
#include <string>

namespace cfgsync::commands {
namespace fs = std::filesystem;

UseCommand::UseCommand(core::Registry& registry, storage::StorageManager& storageManager, core::AppConfig& appConfig)
    : Registry_(registry), StorageManager_(storageManager), AppConfig_(appConfig) {}

void UseCommand::Execute(const fs::path& storageRoot) {
    const auto normalizedStorageRoot = utils::NormalizePath(storageRoot);
    if (normalizedStorageRoot.empty()) {
        throw CommandError{"Storage root must not be empty."};
    }

    std::error_code errorCode;
    const auto storageStatus = fs::status(normalizedStorageRoot, errorCode);
    if (errorCode || !fs::exists(storageStatus)) {
        throw CommandError{"cfgsync storage directory does not exist: " + normalizedStorageRoot.string()};
    }

    if (!fs::is_directory(storageStatus)) {
        throw CommandError{"cfgsync storage path is not a directory: " + normalizedStorageRoot.string()};
    }

    StorageManager_.SetStorageRoot(normalizedStorageRoot);
    Registry_.SetRegistryPath(StorageManager_.GetRegistryPath());

    if (!fs::exists(Registry_.GetRegistryPath())) {
        throw RegistryError{"cfgsync storage does not contain a registry: " + Registry_.GetRegistryPath().string()};
    }

    Registry_.Load();
    if (Registry_.GetStorageRoot() != normalizedStorageRoot) {
        Registry_.SetStorageRoot(normalizedStorageRoot);
        Registry_.Save();
        utils::LogInfo("Updated cfgsync registry storage root to " + normalizedStorageRoot.string());
    }

    utils::EnsureDirectoryExists(normalizedStorageRoot / "files");

    AppConfig_.SetStorageRoot(normalizedStorageRoot);
    AppConfig_.Save();

    utils::LogInfo("Using cfgsync storage at " + normalizedStorageRoot.string());
}

}  // namespace cfgsync::commands
