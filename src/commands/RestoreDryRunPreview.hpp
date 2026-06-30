#pragma once

#include <cstdint>
#include <filesystem>
#include <ios>
#include <memory>
#include <string>
#include <system_error>

namespace cfgsync::commands::detail {

class BinaryInput {
public:
    virtual ~BinaryInput() = default;

    virtual bool IsOpen() const = 0;
    virtual bool Good() const = 0;
    virtual void Read(char* buffer, std::streamsize count) = 0;
    virtual bool Bad() const = 0;
    virtual std::streamsize GCount() const = 0;
};

class DryRunFileOperations {
public:
    virtual ~DryRunFileOperations() = default;

    virtual std::uintmax_t FileSize(const std::filesystem::path& path, std::error_code& errorCode) const = 0;
    virtual std::unique_ptr<BinaryInput> OpenBinaryInput(const std::filesystem::path& path) const = 0;
};

bool FilesHaveSameContents(const std::filesystem::path& firstPath, const std::filesystem::path& secondPath,
                           const DryRunFileOperations& operations);

std::string GetDryRunImpact(const std::filesystem::path& storedPath, const std::filesystem::path& destinationPath);
void PrintDryRunImpact(const std::filesystem::path& storedPath, const std::filesystem::path& destinationPath);

}  // namespace cfgsync::commands::detail
