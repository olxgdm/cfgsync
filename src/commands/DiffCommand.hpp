#pragma once

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

#include <filesystem>
#include <iosfwd>

namespace cfgsync::commands {

class DiffCommand {
public:
    DiffCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void Execute(const std::filesystem::path& filePath, std::ostream& output) const;
    void Execute(const std::filesystem::path& filePath) const;

private:
    core::Registry& Registry_;                 // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
