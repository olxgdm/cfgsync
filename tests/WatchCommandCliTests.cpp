#include "common/CliCommandTestFixture.hpp"
#include "common/CliTestUtils.hpp"
#include "common/GoogleTestMain.hpp"
#include "common/TestFileUtils.hpp"
#include "gtest/gtest.h"

#include <string>

namespace {

class WatchCommandCliTest : public cfgsync::tests::CliCommandTestFixture {
protected:
    cfgsync::tests::CommandResult RunWatchCommand() const { return RunCommand("watch"); }
};

TEST_F(WatchCommandCliTest, WatchOfEmptyRegistrySucceedsClearly) {
    ASSERT_TRUE(RunInitCommand());

    const auto result = RunWatchCommand();

    EXPECT_EQ(result.ExitCode, 0);
    EXPECT_NE(result.Output.find("No files tracked."), std::string::npos);
    EXPECT_TRUE(result.Error.empty());
}

TEST_F(WatchCommandCliTest, MissingAppConfigFailsWithInitGuidance) {
    const auto result = RunWatchCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("cfgsync has not been initialized"), std::string::npos);
    EXPECT_NE(result.Error.find("cfgsync init --storage <dir>"), std::string::npos);
}

TEST_F(WatchCommandCliTest, MalformedRegistryFailsClearly) {
    ASSERT_TRUE(RunInitCommand());
    cfgsync::tests::WriteTextFile(StorageRoot() / "registry.json", "{ invalid json");

    const auto result = RunWatchCommand();

    EXPECT_NE(result.ExitCode, 0);
    EXPECT_TRUE(result.Output.empty());
    EXPECT_NE(result.Error.find("Malformed cfgsync registry"), std::string::npos);
}

}  // namespace

int main(int argc, char** argv) { return cfgsync::tests::RunCfgsyncCliGoogleTests(argc, argv); }
