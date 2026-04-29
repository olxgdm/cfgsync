#include "commands/AddCommand.hpp"

#include "utils/FileUtils.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <stdexcept>

namespace cfgsync::commands {

AddCommand::AddCommand(core::Registry& registry) : Registry_(registry) {}

void AddCommand::Execute(const std::filesystem::path& filePath) const {
    const auto normalizedPath = utils::NormalizePath(filePath);
    utils::RequireOrdinaryFile(normalizedPath);

    const auto storedRelativePath = utils::MakeStorageRelativePath(normalizedPath);
    if (storedRelativePath.empty()) {
        throw std::logic_error{"Unable to derive a storage path for: " + normalizedPath.string()};
    }

    const auto added = Registry_.AddEntry({
        .OriginalPath = normalizedPath.string(),
        .StoredRelativePath = storedRelativePath.generic_string(),
    });

    if (!added) {
        utils::LogInfo("File is already tracked: " + normalizedPath.string());
        return;
    }

    Registry_.Save();
    utils::LogInfo("Tracking file: " + normalizedPath.string());
}

}  // namespace cfgsync::commands
