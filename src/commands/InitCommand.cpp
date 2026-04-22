#include "commands/InitCommand.hpp"

#include <stdexcept>

#include "utils/PathUtils.hpp"

namespace cfgsync::commands {

InitCommand::InitCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void InitCommand::Execute(const std::filesystem::path& storageRoot) {
    const auto normalizedStorageRoot = utils::NormalizePath(storageRoot);
    StorageManager_.SetStorageRoot(normalizedStorageRoot);
    Registry_.SetRegistryPath(StorageManager_.GetRegistryPath());

    throw std::logic_error("The 'init' command is wired, but storage initialization is not implemented yet. "
                           "Target: " +
                           normalizedStorageRoot.string());
}

}  // namespace cfgsync::commands
