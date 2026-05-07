#pragma once

#include "core/Registry.hpp"
#include "diff/FileComparator.hpp"
#include "storage/StorageManager.hpp"

#include <iosfwd>

namespace cfgsync::commands {

class StatusCommand {
public:
    StatusCommand(core::Registry& registry, storage::StorageManager& storageManager);

    void Execute(std::ostream& output) const;
    void Execute() const;

private:
    core::Registry& Registry_;                 // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    storage::StorageManager& StorageManager_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
