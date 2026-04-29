#pragma once

#include "core/AppConfig.hpp"
#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

namespace cfgsync {

class App {
public:
    int Run(int argc, char* argv[]);

private:
    core::AppConfig AppConfig_;
    core::Registry Registry_;
    storage::StorageManager StorageManager_;
};

}  // namespace cfgsync
