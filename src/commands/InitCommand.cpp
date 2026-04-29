#include "commands/InitCommand.hpp"

#include <string>

#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

namespace cfgsync::commands {

InitCommand::InitCommand(core::Registry& registry, storage::StorageManager& storageManager, core::AppConfig& appConfig)
    : Registry_(registry), StorageManager_(storageManager), AppConfig_(appConfig) {}

void InitCommand::Execute(const std::filesystem::path& storageRoot) {
    const auto normalizedStorageRoot = utils::NormalizePath(storageRoot);
    StorageManager_.SetStorageRoot(normalizedStorageRoot);
    Registry_.SetRegistryPath(StorageManager_.GetRegistryPath());

    Registry_.Initialize(normalizedStorageRoot);
    AppConfig_.SetStorageRoot(normalizedStorageRoot);
    AppConfig_.Save();

    utils::LogInfo(std::string{"Initialized cfgsync storage at "} + normalizedStorageRoot.string());
}

}  // namespace cfgsync::commands
