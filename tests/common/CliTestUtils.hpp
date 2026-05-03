#pragma once

#include <filesystem>
#include <string>

namespace cfgsync::tests {

struct CommandResult {
    int ExitCode;
    std::string Output;
    std::string Error;
};

void InitializeCfgsyncExecutablePath(const char* testExecutablePath);
void SetEnvironmentVariable(const std::string& name, const std::string& value);

std::string QuoteForCommand(const std::filesystem::path& path);
CommandResult RunCfgsyncCommand(const std::string& arguments, const std::filesystem::path& outputPath,
                                const std::filesystem::path& errorPath);
bool CfgsyncCommandSucceeded(const std::string& arguments, const std::filesystem::path& testRoot);

}  // namespace cfgsync::tests
