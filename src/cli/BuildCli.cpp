#include "cli/BuildCli.hpp"

#include "commands/AddCommand.hpp"
#include "commands/BackupCommand.hpp"
#include "commands/InitCommand.hpp"
#include "commands/ListCommand.hpp"
#include "commands/RemoveCommand.hpp"
#include "commands/RestoreCommand.hpp"
#include "utils/LogUtils.hpp"

namespace cfgsync::cli {

void BuildCli(CLI::App& app,
              core::Registry& registry,
              storage::StorageManager& storageManager,
              core::AppConfig& appConfig) {
    const auto loadActiveStorage = [&registry, &storageManager, &appConfig]() {
        appConfig.Load();
        storageManager.SetStorageRoot(appConfig.GetStorageRoot());
        registry.SetRegistryPath(storageManager.GetRegistryPath());
        utils::LogDebug(std::string{"Resolved active cfgsync storage root: "} + storageManager.GetStorageRoot().string());
    };

    auto* initCommand = app.add_subcommand("init", "Initialize the cfgsync storage directory.");
    auto storageRoot = std::make_shared<std::string>();
    initCommand->add_option("--storage", *storageRoot, "Directory where cfgsync data will be stored.")
        ->required();
    initCommand->callback([&registry, &storageManager, &appConfig, storageRoot]() {
        commands::InitCommand command{registry, storageManager, appConfig};
        command.Execute(std::filesystem::path{*storageRoot});
    });

    auto* addCommand = app.add_subcommand("add", "Register a file for tracking.");
    auto addFile = std::make_shared<std::string>();
    addCommand->add_option("file", *addFile, "Path to the file that should be tracked.")->required();
    addCommand->callback([&registry, loadActiveStorage, addFile]() {
        loadActiveStorage();
        commands::AddCommand command{registry};
        command.Execute(std::filesystem::path{*addFile});
    });

    auto* removeCommand = app.add_subcommand("remove", "Remove a file from the registry.");
    auto removeFile = std::make_shared<std::string>();
    removeCommand->add_option("file", *removeFile, "Tracked file path to remove from the registry.")
        ->required();
    removeCommand->callback([&registry, loadActiveStorage, removeFile]() {
        loadActiveStorage();
        commands::RemoveCommand command{registry};
        command.Execute(std::filesystem::path{*removeFile});
    });

    auto* listCommand = app.add_subcommand("list", "List all tracked files.");
    listCommand->callback([&registry, loadActiveStorage]() {
        loadActiveStorage();
        const commands::ListCommand command{registry};
        command.Execute();
    });

    auto* backupCommand = app.add_subcommand("backup", "Copy tracked files into storage.");
    backupCommand->callback([&registry, &storageManager, loadActiveStorage]() {
        loadActiveStorage();
        const commands::BackupCommand command{registry, storageManager};
        command.Execute();
    });

    auto* restoreCommand = app.add_subcommand("restore", "Restore one tracked file or all tracked files.");
    auto restoreAll = std::make_shared<bool>(false);
    auto restoreFile = std::make_shared<std::string>();
    restoreCommand->add_flag("--all", *restoreAll, "Restore every tracked file.");
    restoreCommand->add_option("file", *restoreFile, "Tracked file path to restore.");
    restoreCommand->callback([&registry, &storageManager, loadActiveStorage, restoreAll, restoreFile]() {
        if (*restoreAll && !restoreFile->empty()) {
            throw std::runtime_error("Specify either '--all' or a single file path, not both.");
        }

        if (!*restoreAll && restoreFile->empty()) {
            throw std::runtime_error("Specify either '--all' or a single file path to restore.");
        }

        loadActiveStorage();
        const commands::RestoreCommand command{registry, storageManager};

        if (*restoreAll) {
            command.ExecuteAll();
            return;
        }

        command.ExecuteSingle(std::filesystem::path{*restoreFile});
    });
}

}  // namespace cfgsync::cli
