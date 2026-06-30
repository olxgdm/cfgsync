#include "commands/RestoreDryRunPreview.hpp"

#include "Exceptions.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace cfgsync::commands::detail {
namespace fs = std::filesystem;

namespace {

class FileBinaryInput final : public BinaryInput {
public:
    explicit FileBinaryInput(const fs::path& path) : Input_(path, std::ios::binary) {}

    bool IsOpen() const override { return static_cast<bool>(Input_); }
    bool Good() const override { return Input_.good(); }
    void Read(char* buffer, std::streamsize count) override { Input_.read(buffer, count); }
    bool Bad() const override { return Input_.bad(); }
    std::streamsize GCount() const override { return Input_.gcount(); }

private:
    std::ifstream Input_;
};

class FileSystemDryRunFileOperations final : public DryRunFileOperations {
public:
    std::uintmax_t FileSize(const fs::path& path, std::error_code& errorCode) const override {
        return fs::file_size(path, errorCode);
    }

    std::unique_ptr<BinaryInput> OpenBinaryInput(const fs::path& path) const override {
        return std::make_unique<FileBinaryInput>(path);
    }
};

[[noreturn]] void ThrowOpenFailure(const fs::path& path) {
    throw FileError{std::format("Unable to open file '{}'", path.string())};
}

const DryRunFileOperations& FileSystemOperations() {
    static const FileSystemDryRunFileOperations operations;
    return operations;
}

}  // namespace

bool FilesHaveSameContents(const fs::path& firstPath, const fs::path& secondPath,
                           const DryRunFileOperations& operations) {
    std::error_code errorCode;
    const auto firstSize = operations.FileSize(firstPath, errorCode);
    if (errorCode) {
        throw FileError{std::format("Unable to inspect file '{}': {}", firstPath.string(), errorCode.message())};
    }

    const auto secondSize = operations.FileSize(secondPath, errorCode);
    if (errorCode) {
        throw FileError{std::format("Unable to inspect file '{}': {}", secondPath.string(), errorCode.message())};
    }

    if (firstSize != secondSize) {
        return false;
    }

    auto first = operations.OpenBinaryInput(firstPath);
    if (!first->IsOpen()) {
        ThrowOpenFailure(firstPath);
    }

    auto second = operations.OpenBinaryInput(secondPath);
    if (!second->IsOpen()) {
        ThrowOpenFailure(secondPath);
    }

    std::array<char, 8192> firstBuffer{};
    std::array<char, 8192> secondBuffer{};

    while (first->Good() && second->Good()) {
        first->Read(firstBuffer.data(), static_cast<std::streamsize>(firstBuffer.size()));
        second->Read(secondBuffer.data(), static_cast<std::streamsize>(secondBuffer.size()));

        if (first->Bad()) {
            throw FileError{std::format("Unable to read file '{}'", firstPath.string())};
        }

        if (second->Bad()) {
            throw FileError{std::format("Unable to read file '{}'", secondPath.string())};
        }

        const auto bytesRead = first->GCount();
        if (bytesRead != second->GCount()) {
            return false;
        }

        if (!std::equal(firstBuffer.begin(), firstBuffer.begin() + bytesRead, secondBuffer.begin())) {
            return false;
        }
    }

    return true;
}

std::string GetDryRunImpact(const fs::path& storedPath, const fs::path& destinationPath) {
    if (destinationPath.empty()) {
        throw FileError{"Destination path must not be empty."};
    }

    std::error_code errorCode;
    const auto destinationStatus = fs::symlink_status(destinationPath, errorCode);
    if (errorCode) {
        if (errorCode == std::errc::no_such_file_or_directory) {
            return "would-create";
        }

        throw FileError{
            std::format("Unable to inspect destination '{}': {}", destinationPath.string(), errorCode.message())};
    }

    if (!fs::exists(destinationStatus)) {
        return "would-create";
    }

    if (destinationStatus.type() != fs::file_type::regular) {
        throw FileError{std::format("Destination path is not an ordinary file: {}", destinationPath.string())};
    }

    if (FilesHaveSameContents(storedPath, destinationPath, FileSystemOperations())) {
        return "unchanged";
    }

    return "would-overwrite";
}

void PrintDryRunImpact(const fs::path& storedPath, const fs::path& destinationPath) {
    std::cout << GetDryRunImpact(storedPath, destinationPath) << ' ' << destinationPath.string() << '\n';
}

}  // namespace cfgsync::commands::detail
