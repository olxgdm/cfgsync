#include "core/Registry.hpp"

#include "utils/FileUtils.hpp"
#include "utils/LogUtils.hpp"
#include "utils/PathUtils.hpp"

#include <fmt/format.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace cfgsync::core {
namespace fs = std::filesystem;

namespace {

constexpr int CurrentRegistryVersion = 1;

}  // namespace

Registry::Registry(fs::path registryPath) : RegistryPath_(std::move(registryPath)) {}

void Registry::SetRegistryPath(fs::path registryPath) { RegistryPath_ = std::move(registryPath); }

const fs::path& Registry::GetRegistryPath() const { return RegistryPath_; }

void Registry::Initialize(const fs::path& storageRoot) {
    const auto normalizedStorageRoot = utils::NormalizePath(storageRoot);
    if (normalizedStorageRoot.empty()) {
        throw std::invalid_argument{"Storage root must not be empty."};
    }

    if (RegistryPath_.empty()) {
        RegistryPath_ = normalizedStorageRoot / "registry.json";
    }

    utils::EnsureDirectoryExists(normalizedStorageRoot / "files");

    if (fs::exists(RegistryPath_)) {
        utils::LogInfo(std::string{"Using existing cfgsync registry at "} + RegistryPath_.string());
        return;
    }

    if (RegistryPath_.has_parent_path()) {
        utils::EnsureDirectoryExists(RegistryPath_.parent_path());
    }

    const nlohmann::json document = {
        {"version", CurrentRegistryVersion},
        {"storage_root", normalizedStorageRoot.string()},
        {"tracked_files", nlohmann::json::array()},
    };

    std::ofstream output{RegistryPath_};
    if (!output) {
        throw std::runtime_error{
            fmt::format(fmt::runtime("Unable to write cfgsync registry: {}"), RegistryPath_.string())};
    }

    output << document.dump(4) << '\n';
    utils::LogInfo(std::string{"Created cfgsync registry at "} + RegistryPath_.string());
}

const std::vector<TrackedEntry>& Registry::GetTrackedEntries() const { return TrackedEntries_; }

}  // namespace cfgsync::core
