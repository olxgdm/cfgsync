#pragma once

#include "core/Registry.hpp"

#include <filesystem>

namespace cfgsync::commands {

class RemoveCommand {
public:
    explicit RemoveCommand(core::Registry& registry);

    void Execute(const std::filesystem::path& filePath);

private:
    core::Registry& Registry_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
