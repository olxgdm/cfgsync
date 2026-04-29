#pragma once

#include <CLI/CLI.hpp>

#include "core/AppConfig.hpp"
#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

namespace cfgsync::cli {

void BuildCli(CLI::App& app,
              core::Registry& registry,
              storage::StorageManager& storageManager,
              core::AppConfig& appConfig);

}  // namespace cfgsync::cli
