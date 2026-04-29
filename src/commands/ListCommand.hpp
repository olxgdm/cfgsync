#pragma once

#include "core/Registry.hpp"

namespace cfgsync::commands {

class ListCommand {
public:
    explicit ListCommand(core::Registry& registry);

    void Execute() const;

private:
    core::Registry& Registry_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
