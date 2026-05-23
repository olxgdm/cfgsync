#pragma once

#include "core/Registry.hpp"

#include <filesystem>

namespace cfgsync::commands {

class AddCommand {
public:
    explicit AddCommand(core::Registry& registry);

    void Execute(const std::filesystem::path& filePath) const;

private:
    bool AddFileEntry(const std::filesystem::path& normalizedPath) const;
    void ExecuteFile(const std::filesystem::path& normalizedPath) const;
    void ExecuteDirectory(const std::filesystem::path& normalizedPath) const;

    core::Registry& Registry_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

}  // namespace cfgsync::commands
