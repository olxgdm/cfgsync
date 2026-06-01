#include "commands/BackupCommand.hpp"

#include "Exceptions.hpp"
#include "utils/LogUtils.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <memory>
#include <openssl/evp.h>
#include <sstream>
#include <string>
#include <string_view>

namespace cfgsync::commands {
namespace fs = std::filesystem;

namespace {

using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

void RequireOpenSslSuccess(int result, std::string_view failureMessage) {
    if (result != 1) {
        throw FileError{std::string{failureMessage}};
    }
}

DigestContext CreateDigestContext() {
    DigestContext context{EVP_MD_CTX_new(), &EVP_MD_CTX_free};
    if (context == nullptr) {
        throw FileError{"Unable to create SHA-256 context"};
    }

    return context;
}

std::string CalculateSha256(const fs::path& path) {
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        throw FileError{std::format("Unable to open file '{}'", path.string())};
    }

    auto context = CreateDigestContext();
    RequireOpenSslSuccess(EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr), "Unable to initialize SHA-256");

    std::array<char, 8192> buffer{};

    while (file.good()) {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto bytesRead = file.gcount();

        if (bytesRead > 0) {
            RequireOpenSslSuccess(EVP_DigestUpdate(context.get(), buffer.data(), static_cast<std::size_t>(bytesRead)),
                                  std::format("Unable to hash file '{}'", path.string()));
        }
    }

    if (file.bad()) {
        throw FileError{std::format("Unable to read file '{}'", path.string())};
    }

    std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
    unsigned int hashLength = 0;

    RequireOpenSslSuccess(EVP_DigestFinal_ex(context.get(), hash.data(), &hashLength), "Unable to finalize SHA-256");

    std::ostringstream result;
    result << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < hashLength; ++i) {
        result << std::setw(2) << static_cast<int>(hash[i]);
    }

    return result.str();
}

bool StoredBackupIsUpToDate(const fs::path& originalPath, const fs::path& storedPath) {
    std::error_code errorCode;

    const auto exists = fs::exists(storedPath, errorCode);
    if (errorCode) {
        throw FileError{
            std::format("Unable to inspect stored backup '{}': {}", storedPath.string(), errorCode.message())};
    }

    if (!exists) {
        return false;
    }

    const auto originalChecksum = CalculateSha256(originalPath);
    const auto storedChecksum = CalculateSha256(storedPath);

    return originalChecksum == storedChecksum;
}

}  // namespace

BackupCommand::BackupCommand(core::Registry& registry, storage::StorageManager& storageManager)
    : Registry_(registry), StorageManager_(storageManager) {}

void BackupCommand::Execute() const {
    const auto& trackedEntries = Registry_.GetTrackedEntries();

    if (trackedEntries.empty()) {
        utils::LogInfo("No files tracked.");
        return;
    }

    std::size_t failureCount = 0;
    std::size_t backupCount = 0;

    for (const auto& trackedEntry : trackedEntries) {
        try {
            if (const auto storedPath = StorageManager_.ResolveStoredPath(trackedEntry);
                StoredBackupIsUpToDate(trackedEntry.OriginalPath, storedPath)) {
                continue;
            }

            StorageManager_.BackupEntry(trackedEntry);

            ++backupCount;

            utils::LogInfo("Backed up file: " + trackedEntry.OriginalPath);
        } catch (const FileError& error) {
            ++failureCount;

            utils::LogWarn("Failed to back up file: " + trackedEntry.OriginalPath + ": " + error.what());
        }
    }

    if (failureCount > 0) {
        throw CommandError{
            std::format("Backup completed with {} failure{}.", failureCount, failureCount == 1 ? "" : "s")};
    }

    if (backupCount == 0) {
        utils::LogInfo("No new files to back up.");
    }
}

}  // namespace cfgsync::commands
