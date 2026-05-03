#include "commands/RemoveCommand.hpp"

#include "Exceptions.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

namespace cfgsync::commands {

RemoveCommand::RemoveCommand(core::Registry& registry) : Registry_(registry) {}

void RemoveCommand::Execute(const std::filesystem::path& filePath) {
    const auto normalizedPath = utils::NormalizePath(filePath);
    const auto removed = Registry_.RemoveEntry(normalizedPath);
    if (!removed) {
        throw CommandError{"File is not tracked: " + normalizedPath.string()};
    }

    Registry_.Save();
    utils::LogInfo("Removed file from tracking: " + normalizedPath.string());
}

}  // namespace cfgsync::commands
