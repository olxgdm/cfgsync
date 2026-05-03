#pragma once

#include "CliTestUtils.hpp"
#include "TestTempDirectory.hpp"

#include "gtest/gtest.h"

#include <filesystem>
#include <string>

namespace cfgsync::tests {

class CliCommandTestFixture : public testing::Test {
protected:
    void SetUp() override {
        TestRoot_ = MakeTestRoot();
#ifdef _WIN32
        SetEnvironmentVariable("APPDATA", (TestRoot_ / "appdata").string());
#else
        SetEnvironmentVariable("HOME", (TestRoot_ / "home").string());
#endif
    }

    void TearDown() override { std::filesystem::remove_all(TestRoot_); }

    std::filesystem::path GetTestRoot() const { return TestRoot_; }

    std::filesystem::path StorageRoot() const { return TestRoot_ / "storage"; }

    std::filesystem::path SourcePath(const std::string& filename) const { return TestRoot_ / "configs" / filename; }

    bool RunInitCommand() const {
        return CfgsyncCommandSucceeded("init --storage " + QuoteForCommand(StorageRoot()), GetTestRoot());
    }

    bool RunAddCommand(const std::filesystem::path& sourcePath) const {
        return CfgsyncCommandSucceeded("add " + QuoteForCommand(sourcePath), GetTestRoot());
    }

    CommandResult RunCommand(const std::string& arguments) const {
        return RunCfgsyncCommand(arguments, GetTestRoot() / "command.out", GetTestRoot() / "command.err");
    }

private:
    std::filesystem::path TestRoot_;
};

}  // namespace cfgsync::tests
