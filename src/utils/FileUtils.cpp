#include "utils/FileUtils.hpp"

#include <stdexcept>

namespace cfgsync::utils {
namespace fs = std::filesystem;

void EnsureDirectoryExists(const fs::path& path) {
    if (path.empty()) {
        return;
    }

    fs::create_directories(path);
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

    fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
}

}  // namespace cfgsync::utils
