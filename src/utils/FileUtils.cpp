#include "utils/FileUtils.hpp"

#include "Exceptions.hpp"

#include <fmt/format.h>
#include <stdexcept>

namespace cfgsync::utils {
namespace fs = std::filesystem;

void EnsureDirectoryExists(const fs::path& path) {
    if (path.empty()) {
        return;
    }

    std::error_code errorCode;
    fs::create_directories(path, errorCode);
    if (errorCode) {
        throw FileError{
            fmt::format(fmt::runtime("Unable to create directory '{}': {}"), path.string(), errorCode.message())};
    }
}

bool IsOrdinaryFile(const fs::path& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code errorCode;
    const auto status = fs::symlink_status(path, errorCode);
    if (errorCode || !fs::exists(status)) {
        return false;
    }

    return status.type() == fs::file_type::regular;
}

void RequireOrdinaryFile(const fs::path& path) {
    if (path.empty()) {
        throw std::invalid_argument{"Path must not be empty."};
    }

    std::error_code errorCode;
    const auto status = fs::symlink_status(path, errorCode);
    if (errorCode) {
        if (errorCode == std::errc::no_such_file_or_directory) {
            throw FileError{fmt::format(fmt::runtime("Path does not exist: {}"), path.string())};
        }

        throw FileError{
            fmt::format(fmt::runtime("Unable to inspect file '{}': {}"), path.string(), errorCode.message())};
    }

    if (!fs::exists(status)) {
        throw FileError{fmt::format(fmt::runtime("Path does not exist: {}"), path.string())};
    }

    if (status.type() != fs::file_type::regular) {
        throw FileError{fmt::format(fmt::runtime("Path is not an ordinary file: {}"), path.string())};
    }
}

void CopyFile(const fs::path& source, const fs::path& destination) {
    if (source.empty()) {
        throw std::invalid_argument("Source path must not be empty.");
    }

    if (destination.empty()) {
        throw std::invalid_argument("Destination path must not be empty.");
    }

    if (destination.has_parent_path()) {
        EnsureDirectoryExists(destination.parent_path());
    }

    std::error_code errorCode;
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, errorCode);
    if (errorCode) {
        throw FileError{fmt::format(fmt::runtime("Unable to copy '{}' to '{}': {}"), source.string(),
                                    destination.string(), errorCode.message())};
    }
}

}  // namespace cfgsync::utils
