#include "utils/AppConfigPath.hpp"

#include "Exceptions.hpp"

#include <cstdlib>
#include <string_view>

namespace cfgsync::utils {
namespace fs = std::filesystem;

fs::path GetDefaultAppConfigPath() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData == nullptr || std::string_view{appData}.empty()) {
        throw ConfigError{"Unable to locate cfgsync app config: APPDATA is not set."};
    }

    return fs::path{appData} / "cfgsync" / "config.json";
#else
    const char* home = std::getenv("HOME");
    if (home == nullptr || std::string_view{home}.empty()) {
        throw ConfigError{"Unable to locate cfgsync app config: HOME is not set."};
    }

    return fs::path{home} / ".config" / "cfgsync" / "config.json";
#endif
}

}  // namespace cfgsync::utils
