#include "commands/ListCommand.hpp"

#include <iostream>
#include <ostream>

namespace cfgsync::commands {

ListCommand::ListCommand(core::Registry& registry) : Registry_(registry) {}

void ListCommand::Execute(std::ostream& output) const {
    const auto& trackedEntries = Registry_.GetTrackedEntries();
    if (trackedEntries.empty()) {
        output << "No files tracked.\n";
        return;
    }

    for (const auto& trackedEntry : trackedEntries) {
        output << trackedEntry.OriginalPath << '\n';
    }
}

void ListCommand::Execute() const { Execute(std::cout); }

}  // namespace cfgsync::commands
