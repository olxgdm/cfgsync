#include "TestTempDirectory.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace cfgsync::tests {
namespace fs = std::filesystem;

std::filesystem::path MakeTestRoot() {
    const auto base = fs::temp_directory_path();  // NOSONAR: Safe for tests; the directory is created with a unique
                                                  // name and restricted owner permissions.

    const auto pid =
#ifdef _WIN32
        static_cast<unsigned long>(_getpid());
#else
        static_cast<unsigned long>(getpid());
#endif

    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto candidate =
            base / ("cfgsync-tests-" + std::to_string(pid) + "-" + std::to_string(now) + "-" + std::to_string(attempt));

        std::error_code errorCode;
        if (!fs::create_directory(candidate, errorCode)) {
            continue;
        }

        fs::permissions(candidate, fs::perms::owner_all, fs::perm_options::replace, errorCode);
        if (errorCode) {
            fs::remove_all(candidate);
            continue;
        }

        return candidate;
    }

    throw std::runtime_error{"Failed to create private temporary test directory"};
}

}  // namespace cfgsync::tests
