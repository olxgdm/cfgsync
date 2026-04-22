#pragma once

#include "core/Registry.hpp"
#include "storage/StorageManager.hpp"

namespace cfgsync {

class App {
public:
    int Run(int argc, char* argv[]);

private:
    core::Registry Registry_;
    storage::StorageManager StorageManager_;
};

}  // namespace cfgsync
