#pragma once

#include "TestTempDirectory.hpp"
#include "core/Registry.hpp"

#include "gtest/gtest.h"

#include <filesystem>
#include <string>

namespace cfgsync::tests {

class RegistryCommandTestFixture : public testing::Test {
protected:
    void SetUp() override {
        TestRoot_ = MakeTestRoot();
        Registry_.SetStorageRoot(StorageRoot());
        Registry_.SetRegistryPath(RegistryPath());
        Registry_.Save();
    }

    void TearDown() override { std::filesystem::remove_all(TestRoot_); }

    std::filesystem::path SourcePath() const { return SourcePath(".gitconfig"); }

    std::filesystem::path SourcePath(const std::string& filename) const {
        return TestRoot_ / "home" / "user" / filename;
    }

    std::filesystem::path StorageRoot() const { return TestRoot_ / "storage"; }

    std::filesystem::path RegistryPath() const { return StorageRoot() / "registry.json"; }

    cfgsync::core::Registry& Registry() { return Registry_; }

private:
    std::filesystem::path TestRoot_;
    cfgsync::core::Registry Registry_;
};

}  // namespace cfgsync::tests
