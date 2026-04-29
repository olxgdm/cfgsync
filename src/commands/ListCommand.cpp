#include "commands/ListCommand.hpp"

#include <stdexcept>

namespace cfgsync::commands {

ListCommand::ListCommand(core::Registry& registry) : Registry_(registry) {}

void ListCommand::Execute() const {
    const auto registryPath =
        Registry_.GetRegistryPath().empty() ? std::string{"<unset>"} : Registry_.GetRegistryPath().string();
    throw std::logic_error(
        "The 'list' command is wired, but registry enumeration is not implemented yet. "
        "Registry: " +
        registryPath);
}

}  // namespace cfgsync::commands
