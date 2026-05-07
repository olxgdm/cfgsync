#include "diff/FileComparator.hpp"

#include "Exceptions.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <ios>
#include <system_error>

namespace cfgsync::diff {
namespace fs = std::filesystem;

namespace {

constexpr std::size_t BufferSize = 8192;

enum class PathState {
    Missing,
    OrdinaryFile,
};

FileError FormatFileError(const fs::path& path, const std::string& message) {
    return FileError{fmt::format(fmt::runtime("{}: {}"), message, path.string())};
}

PathState InspectPath(const fs::path& path) {
    std::error_code errorCode;
    const auto status = fs::symlink_status(path, errorCode);
    if (errorCode) {
        if (errorCode == std::errc::no_such_file_or_directory) {
            return PathState::Missing;
        }

        throw FileError{
            fmt::format(fmt::runtime("Unable to inspect file '{}': {}"), path.string(), errorCode.message())};
    }

    if (!fs::exists(status)) {
        return PathState::Missing;
    }

    if (status.type() != fs::file_type::regular) {
        throw FormatFileError(path, "Path is not an ordinary file");
    }

    return PathState::OrdinaryFile;
}

std::uintmax_t FileSize(const fs::path& path) {
    std::error_code errorCode;
    const auto size = fs::file_size(path, errorCode);
    if (errorCode) {
        throw FileError{
            fmt::format(fmt::runtime("Unable to read file size '{}': {}"), path.string(), errorCode.message())};
    }

    return size;
}

std::ifstream OpenBinaryInput(const fs::path& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw FormatFileError(path, "Unable to open file for comparison");
    }

    return input;
}

bool HaveMatchingBytes(const fs::path& originalPath, const fs::path& storedPath) {
    if (FileSize(originalPath) != FileSize(storedPath)) {
        return false;
    }

    auto originalInput = OpenBinaryInput(originalPath);
    auto storedInput = OpenBinaryInput(storedPath);
    std::array<char, BufferSize> originalBuffer{};
    std::array<char, BufferSize> storedBuffer{};

    while (originalInput && storedInput) {
        originalInput.read(originalBuffer.data(), originalBuffer.size());
        storedInput.read(storedBuffer.data(), storedBuffer.size());

        if (originalInput.gcount() != storedInput.gcount()) {
            return false;
        }

        if (!std::equal(originalBuffer.begin(), originalBuffer.begin() + originalInput.gcount(), storedBuffer.begin())) {
            return false;
        }
    }

    return originalInput.eof() && storedInput.eof();
}

}  // namespace

FileComparator::FileComparator(const storage::StorageManager& storageManager) : StorageManager_(storageManager) {}

FileStatusResult FileComparator::Compare(const core::TrackedEntry& entry) const {
    const fs::path originalPath{entry.OriginalPath};
    const auto storedPath = StorageManager_.ResolveStoredPath(entry);

    if (InspectPath(originalPath) == PathState::Missing) {
        return {
            .Status = FileStatus::MissingOriginal,
            .Entry = entry,
        };
    }

    if (InspectPath(storedPath) == PathState::Missing) {
        return {
            .Status = FileStatus::MissingBackup,
            .Entry = entry,
        };
    }

    return {
        .Status = HaveMatchingBytes(originalPath, storedPath) ? FileStatus::Clean : FileStatus::Modified,
        .Entry = entry,
    };
}

std::vector<FileStatusResult> FileComparator::CompareAll(const std::vector<core::TrackedEntry>& entries) const {
    std::vector<FileStatusResult> results;
    results.reserve(entries.size());

    for (const auto& entry : entries) {
        results.push_back(Compare(entry));
    }

    return results;
}

}  // namespace cfgsync::diff
