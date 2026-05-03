#pragma once

#include "CliTestUtils.hpp"

#include "gtest/gtest.h"

namespace cfgsync::tests {

inline int RunGoogleTests(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

inline int RunCfgsyncCliGoogleTests(int argc, char** argv) {
    if (argc > 0) {
        InitializeCfgsyncExecutablePath(argv[0]);
    }

    return RunGoogleTests(argc, argv);
}

}  // namespace cfgsync::tests
