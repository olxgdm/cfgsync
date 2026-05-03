#include "CliTestUtils.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace cfgsync::tests {
namespace fs = std::filesystem;

namespace {

constexpr const char* CfgsyncExecutablePathVariable = "CFGSYNC_TEST_EXECUTABLE_PATH";

fs::path ResolveCfgsyncExecutablePath(const char* testExecutablePath) {
    auto executablePath = fs::absolute(fs::path{testExecutablePath}).parent_path() / "cfgsync";
#ifdef _WIN32
    executablePath += ".exe";
#endif
    return executablePath;
}

std::string ReadTextFile(const fs::path& path) {
    std::ifstream input{path};
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

fs::path GetCfgsyncExecutablePath() {
    const auto* executablePath = std::getenv(CfgsyncExecutablePathVariable);
    if (executablePath == nullptr || std::string{executablePath}.empty()) {
        throw std::runtime_error{"cfgsync executable path is not initialized for CLI tests."};
    }

    return fs::path{executablePath};
}

}  // namespace

void InitializeCfgsyncExecutablePath(const char* testExecutablePath) {
    SetEnvironmentVariable(CfgsyncExecutablePathVariable, ResolveCfgsyncExecutablePath(testExecutablePath).string());
}

void SetEnvironmentVariable(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

std::string QuoteForCommand(const fs::path& path) { return "\"" + path.string() + "\""; }

CommandResult RunCfgsyncCommand(const std::string& arguments, const fs::path& outputPath, const fs::path& errorPath) {
    auto command = QuoteForCommand(GetCfgsyncExecutablePath()) + " " + arguments + " > " + QuoteForCommand(outputPath) +
                   " 2> " + QuoteForCommand(errorPath);
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif

    const auto exitCode = std::system(command.c_str());  // NOSONAR
    return CommandResult{
        .ExitCode = exitCode,
        .Output = ReadTextFile(outputPath),
        .Error = ReadTextFile(errorPath),
    };
}

bool CfgsyncCommandSucceeded(const std::string& arguments, const fs::path& testRoot) {
    const auto result = RunCfgsyncCommand(arguments, testRoot / "command.out", testRoot / "command.err");
    return result.ExitCode == 0;
}

}  // namespace cfgsync::tests
