#include "commands/AddCommand.hpp"

#include "utils/FileUtils.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

namespace cfgsync::commands {
namespace fs = std::filesystem;

namespace {

bool IsDirectory(const fs::path& path) {
    std::error_code errorCode;
    const auto status = fs::symlink_status(path, errorCode);
    return !errorCode && fs::is_directory(status);
}

std::string DescribeSkippedEntry(const fs::path& path, const std::error_code& errorCode) {
    return "Skipping entry '" + path.string() + "': " + errorCode.message();
}

std::string DescribeUnsupportedEntry(const fs::path& path) { return "Skipping unsupported entry: " + path.string(); }

std::string DescribeSymlinkEntry(const fs::directory_entry& entry) {
    std::error_code errorCode;
    const auto targetStatus = entry.status(errorCode);
    if (!errorCode && fs::is_directory(targetStatus)) {
        return "Skipping symlinked directory: " + entry.path().string();
    }

    return "Skipping symlink: " + entry.path().string();
}

void IncrementIterator(fs::recursive_directory_iterator& iterator, const fs::recursive_directory_iterator& end,
                       const fs::path& entryPath) {
    std::error_code errorCode;
    iterator.increment(errorCode);
    if (errorCode) {
        utils::LogWarn(DescribeSkippedEntry(entryPath, errorCode));
        iterator = end;
    }
}

std::vector<fs::path> CollectOrdinaryFiles(const fs::path& directoryPath) {
    std::vector<fs::path> files;

    std::error_code errorCode;
    fs::recursive_directory_iterator iterator{directoryPath, fs::directory_options::skip_permission_denied, errorCode};
    const fs::recursive_directory_iterator end;
    if (errorCode) {
        utils::LogWarn(DescribeSkippedEntry(directoryPath, errorCode));
        return files;
    }

    while (iterator != end) {
        const auto entryPath = iterator->path();
        const auto status = iterator->symlink_status(errorCode);
        if (errorCode) {
            iterator.disable_recursion_pending();
            utils::LogWarn(DescribeSkippedEntry(entryPath, errorCode));
            IncrementIterator(iterator, end, entryPath);
            continue;
        }

        if (fs::is_symlink(status)) {
            iterator.disable_recursion_pending();
            utils::LogWarn(DescribeSymlinkEntry(*iterator));
            IncrementIterator(iterator, end, entryPath);
            continue;
        }

        if (fs::is_directory(status)) {
            IncrementIterator(iterator, end, entryPath);
            continue;
        }

        if (fs::is_regular_file(status)) {
            files.push_back(utils::NormalizePath(entryPath));
            IncrementIterator(iterator, end, entryPath);
            continue;
        }

        utils::LogWarn(DescribeUnsupportedEntry(entryPath));
        IncrementIterator(iterator, end, entryPath);
    }

    std::sort(files.begin(), files.end(), [](const fs::path& left, const fs::path& right) {
        return left.generic_string() < right.generic_string();
    });

    return files;
}

}  // namespace

AddCommand::AddCommand(core::Registry& registry) : Registry_(registry) {}

bool AddCommand::AddFileEntry(const fs::path& normalizedPath) const {
    const auto storedRelativePath = utils::MakeStorageRelativePath(normalizedPath);
    if (storedRelativePath.empty()) {
        throw std::logic_error{"Unable to derive a storage path for: " + normalizedPath.string()};
    }

    const auto added = Registry_.AddEntry({
        .OriginalPath = normalizedPath.string(),
        .StoredRelativePath = storedRelativePath.generic_string(),
    });

    if (!added) {
        utils::LogInfo("File is already tracked: " + normalizedPath.string());
        return false;
    }

    utils::LogInfo("Tracking file: " + normalizedPath.string());
    return true;
}

void AddCommand::ExecuteFile(const fs::path& normalizedPath) const {
    utils::RequireOrdinaryFile(normalizedPath);

    if (AddFileEntry(normalizedPath)) {
        Registry_.Save();
    }
}

void AddCommand::ExecuteDirectory(const fs::path& normalizedPath) const {
    const auto files = CollectOrdinaryFiles(normalizedPath);
    if (files.empty()) {
        utils::LogInfo("No ordinary files found under directory: " + normalizedPath.string());
        return;
    }

    auto addedCount = 0U;
    auto duplicateCount = 0U;
    for (const auto& filePath : files) {
        if (AddFileEntry(filePath)) {
            ++addedCount;
        } else {
            ++duplicateCount;
        }
    }

    if (addedCount == 0U) {
        utils::LogInfo("No new files to track under directory: " + normalizedPath.string());
        return;
    }

    Registry_.Save();
    auto summary = std::format("Imported {} file(s) from directory: {}", addedCount, normalizedPath.string());
    if (duplicateCount > 0U) {
        summary += std::format(" (skipped {} already tracked file(s))", duplicateCount);
    }
    utils::LogInfo(summary);
}

void AddCommand::Execute(const fs::path& path) const {
    const auto normalizedPath = utils::NormalizePath(path);
    if (IsDirectory(normalizedPath)) {
        ExecuteDirectory(normalizedPath);
        return;
    }

    ExecuteFile(normalizedPath);
}

}  // namespace cfgsync::commands
