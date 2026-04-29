#include "App.hpp"

#include "cli/BuildCli.hpp"
#include "utils/AppConfigPath.hpp"

#include <CLI/CLI.hpp>
#include <cstdlib>
#include <exception>
#include <iostream>

namespace cfgsync {

int App::Run(int argc, char* argv[]) {
    CLI::App cli{"cfgsync keeps local backups of text-based configuration files."};
    cli.require_subcommand(1);
    cli.set_help_all_flag("--help-all", "Show help for all commands.");
    cli.set_version_flag("-v,--version", "cfgsync 0.1.0");

    try {
        AppConfig_.SetConfigPath(utils::GetDefaultAppConfigPath());
        cli::BuildCli(cli, Registry_, StorageManager_, AppConfig_);
        cli.parse(argc, argv);
        return EXIT_SUCCESS;
    } catch (const CLI::ParseError& error) {
        return cli.exit(error);
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

}  // namespace cfgsync
