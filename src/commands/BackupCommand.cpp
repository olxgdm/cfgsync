#include "commands/BackupCommand.hpp"

#include <stdexcept>

namespace cfgsync::commands {

BackupCommand::BackupCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void BackupCommand::Execute() const {
    const auto registryPath = Registry_.GetRegistryPath().empty() ? std::string{"<unset>"}
                                                                  : Registry_.GetRegistryPath().string();
    const auto storageRoot = StorageManager_.GetStorageRoot().empty() ? std::string{"<unset>"}
                                                                      : StorageManager_.GetStorageRoot().string();
    throw std::logic_error("The 'backup' command is wired, but backup operations are not implemented yet. "
                           "Registry: " +
                           registryPath + ". Storage: " + storageRoot);
}

}  // namespace cfgsync::commands
