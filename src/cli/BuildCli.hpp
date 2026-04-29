#pragma once

#include "core/AppConfig.hpp"
#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

#include <CLI/CLI.hpp>

namespace cfgsync::cli {

void BuildCli(CLI::App& app, core::Registry& registry, storage::StorageManager& storageManager,
              core::AppConfig& appConfig);

}  // namespace cfgsync::cli
