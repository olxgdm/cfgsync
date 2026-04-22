#pragma once

#include <filesystem>

#include "core/Registry.hpp"

namespace cfgsync::commands {

class RemoveCommand {
public:
    explicit RemoveCommand(core::Registry& registry);

    void Execute(const std::filesystem::path& filePath);

private:
    core::Registry& Registry_;
};

}  // namespace cfgsync::commands
