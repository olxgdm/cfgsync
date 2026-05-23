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

void IncrementIterator(fs::recursive_directory_iterator& iterator, const fs::recursive_directory_iterator& end) {
    std::error_code errorCode;
    iterator.increment(errorCode);
    if (errorCode) {
        utils::LogWarn("Skipping directory entry: " + errorCode.message());
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
            IncrementIterator(iterator, end);
            continue;
        }

        if (fs::is_symlink(status)) {
            iterator.disable_recursion_pending();
            utils::LogWarn("Skipping symlink: " + entryPath.string());
            IncrementIterator(iterator, end);
            continue;
        }

        if (fs::is_directory(status)) {
            IncrementIterator(iterator, end);
            continue;
        }

        if (fs::is_regular_file(status)) {
            files.push_back(utils::NormalizePath(entryPath));
            IncrementIterator(iterator, end);
            continue;
        }

        utils::LogWarn(DescribeUnsupportedEntry(entryPath));
        IncrementIterator(iterator, end);
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
    for (const auto& filePath : files) {
        if (AddFileEntry(filePath)) {
            ++addedCount;
        }
    }

    if (addedCount == 0U) {
        utils::LogInfo("No new files to track under directory: " + normalizedPath.string());
        return;
    }

    Registry_.Save();
    utils::LogInfo(std::format("Imported {} file(s) from directory: {}", addedCount, normalizedPath.string()));
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
