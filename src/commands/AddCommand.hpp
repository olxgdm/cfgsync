#pragma once

#include "core/Registry.hpp"

#include <filesystem>

namespace cfgsync::commands {

class AddCommand {
public:
    explicit AddCommand(core::Registry& registry);

    void Execute(const std::filesystem::path& filePath) const;

private:
    core::Registry& Registry_;
};

}  // namespace cfgsync::commands
