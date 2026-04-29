#include "commands/AddCommand.hpp"

#include "utils/PathUtils.hpp"

#include <stdexcept>

namespace cfgsync::commands {

AddCommand::AddCommand(core::Registry& registry) : Registry_(registry) {}

void AddCommand::Execute(const std::filesystem::path& filePath) {
    const auto normalizedPath = utils::NormalizePath(filePath);
    const auto registryPath =
        Registry_.GetRegistryPath().empty() ? std::string{"<unset>"} : Registry_.GetRegistryPath().string();

    throw std::logic_error("The 'add' command is wired, but registry updates are not implemented yet. File: " +
                           normalizedPath.string() + ". Registry: " + registryPath);
}

}  // namespace cfgsync::commands
