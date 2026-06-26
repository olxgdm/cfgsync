#include "commands/RestoreCommand.hpp"

#include "Exceptions.hpp"
#include "utils/FileUtils.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

namespace cfgsync::commands {
namespace fs = std::filesystem;

namespace {

bool HasPathPrefix(const fs::path& path, const fs::path& prefix) {
    auto pathIterator = path.begin();
    auto prefixIterator = prefix.begin();
    for (; prefixIterator != prefix.end(); ++prefixIterator, ++pathIterator) {
        if (pathIterator == path.end() || *pathIterator != *prefixIterator) {
            return false;
        }
    }

    return true;
}

fs::path RemapDestinationPath(const fs::path& originalPath, const RestorePrefixRemap& remap) {
    if (!HasPathPrefix(originalPath, remap.FromPrefix)) {
        throw CommandError{"Tracked file is outside --from-prefix: " + originalPath.string()};
    }

    auto pathIterator = originalPath.begin();
    for (auto prefixIterator = remap.FromPrefix.begin(); prefixIterator != remap.FromPrefix.end(); ++prefixIterator) {
        ++pathIterator;
    }

    auto destinationPath = remap.ToPrefix;
    for (; pathIterator != originalPath.end(); ++pathIterator) {
        destinationPath /= *pathIterator;
    }

    return destinationPath.lexically_normal();
}

fs::path GetRestoreDestinationPath(const core::TrackedEntry& trackedEntry,
                                   const std::optional<RestorePrefixRemap>& remap) {
    const fs::path originalPath{trackedEntry.OriginalPath};
    if (!remap.has_value()) {
        return originalPath;
    }

    return RemapDestinationPath(originalPath, *remap);
}

void ThrowOpenFailure(const fs::path& path) { throw FileError{std::format("Unable to open file '{}'", path.string())}; }

bool FilesHaveSameContents(const fs::path& firstPath, const fs::path& secondPath) {
    std::error_code errorCode;
    const auto firstSize = fs::file_size(firstPath, errorCode);
    if (errorCode) {
        throw FileError{std::format("Unable to inspect file '{}': {}", firstPath.string(), errorCode.message())};
    }

    const auto secondSize = fs::file_size(secondPath, errorCode);
    if (errorCode) {
        throw FileError{std::format("Unable to inspect file '{}': {}", secondPath.string(), errorCode.message())};
    }

    if (firstSize != secondSize) {
        return false;
    }

    std::ifstream first{firstPath, std::ios::binary};
    if (!first) {
        ThrowOpenFailure(firstPath);
    }

    std::ifstream second{secondPath, std::ios::binary};
    if (!second) {
        ThrowOpenFailure(secondPath);
    }

    std::array<char, 8192> firstBuffer{};
    std::array<char, 8192> secondBuffer{};

    while (first.good() && second.good()) {
        first.read(firstBuffer.data(), static_cast<std::streamsize>(firstBuffer.size()));
        second.read(secondBuffer.data(), static_cast<std::streamsize>(secondBuffer.size()));

        if (first.bad()) {
            throw FileError{std::format("Unable to read file '{}'", firstPath.string())};
        }

        if (second.bad()) {
            throw FileError{std::format("Unable to read file '{}'", secondPath.string())};
        }

        const auto bytesRead = first.gcount();
        if (bytesRead != second.gcount()) {
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

    if (FilesHaveSameContents(storedPath, destinationPath)) {
        return "unchanged";
    }

    return "would-overwrite";
}

void PrintDryRunImpact(const fs::path& storedPath, const fs::path& destinationPath) {
    std::cout << GetDryRunImpact(storedPath, destinationPath) << ' ' << destinationPath.string() << '\n';
}

}  // namespace

RestoreCommand::RestoreCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void RestoreCommand::ExecuteAll(const std::optional<RestorePrefixRemap>& remap, RestoreMode mode) const {
    const auto& trackedEntries = Registry_.GetTrackedEntries();
    if (trackedEntries.empty()) {
        utils::LogInfo("No files tracked.");
        return;
    }

    std::size_t failureCount = 0;
    for (const auto& trackedEntry : trackedEntries) {
        try {
            const auto destinationPath = GetRestoreDestinationPath(trackedEntry, remap);
            if (mode == RestoreMode::DryRun) {
                const auto storedPath = StorageManager_.ResolveStoredPath(trackedEntry);
                utils::RequireOrdinaryFile(storedPath);
                PrintDryRunImpact(storedPath, destinationPath);
                continue;
            }

            StorageManager_.RestoreEntry(trackedEntry, destinationPath);
            utils::LogInfo("Restored file: " + trackedEntry.OriginalPath);
        } catch (const FileError& error) {
            ++failureCount;
            utils::LogWarn("Failed to restore file: " + trackedEntry.OriginalPath + ": " + error.what());
        } catch (const CommandError& error) {
            ++failureCount;
            utils::LogWarn("Failed to restore file: " + trackedEntry.OriginalPath + ": " + error.what());
        }
    }

    if (failureCount > 0) {
        throw CommandError{
            std::format("Restore completed with {} failure{}.", failureCount, failureCount == 1 ? "" : "s")};
    }
}

void RestoreCommand::ExecuteSingle(const std::filesystem::path& filePath,
                                   const std::optional<RestorePrefixRemap>& remap, RestoreMode mode) const {
    const auto normalizedPath = utils::NormalizePath(filePath);
    const auto* trackedEntry = Registry_.FindEntryByOriginalPath(normalizedPath);
    if (trackedEntry == nullptr) {
        throw CommandError{"File is not tracked: " + normalizedPath.string()};
    }

    const auto destinationPath = GetRestoreDestinationPath(*trackedEntry, remap);
    if (mode == RestoreMode::DryRun) {
        const auto storedPath = StorageManager_.ResolveStoredPath(*trackedEntry);
        utils::RequireOrdinaryFile(storedPath);
        PrintDryRunImpact(storedPath, destinationPath);
        return;
    }

    StorageManager_.RestoreEntry(*trackedEntry, destinationPath);
    utils::LogInfo("Restored file: " + trackedEntry->OriginalPath);
}

}  // namespace cfgsync::commands
