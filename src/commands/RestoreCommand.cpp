#include "commands/RestoreCommand.hpp"

#include "utils/PathUtils.hpp"

#include <stdexcept>

namespace cfgsync::commands {

RestoreCommand::RestoreCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void RestoreCommand::ExecuteAll() const {
    const auto registryPath =
        Registry_.GetRegistryPath().empty() ? std::string{"<unset>"} : Registry_.GetRegistryPath().string();
    const auto storageRoot =
        StorageManager_.GetStorageRoot().empty() ? std::string{"<unset>"} : StorageManager_.GetStorageRoot().string();
    throw std::logic_error(
        "The 'restore --all' command is wired, but restore operations are not implemented yet. "
        "Registry: " +
        registryPath + ". Storage: " + storageRoot);
}

void RestoreCommand::ExecuteSingle(const std::filesystem::path& filePath) const {
    const auto normalizedPath = utils::NormalizePath(filePath);
    const auto registryPath =
        Registry_.GetRegistryPath().empty() ? std::string{"<unset>"} : Registry_.GetRegistryPath().string();
    const auto storageRoot =
        StorageManager_.GetStorageRoot().empty() ? std::string{"<unset>"} : StorageManager_.GetStorageRoot().string();

    throw std::logic_error("The 'restore' command is wired, but single-file restore is not implemented yet. File: " +
                           normalizedPath.string() + ". Registry: " + registryPath + ". Storage: " + storageRoot);
}

}  // namespace cfgsync::commands
