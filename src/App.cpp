#include "App.hpp"

#include <cstdlib>
#include <iostream>
#include <exception>

#include <CLI/CLI.hpp>

#include "cli/BuildCli.hpp"

namespace cfgsync {

int App::Run(int argc, char* argv[]) {
    CLI::App cli{"cfgsync keeps local backups of text-based configuration files."};
    cli.require_subcommand(1);
    cli.set_help_all_flag("--help-all", "Show help for all commands.");
    cli.set_version_flag("-v,--version", "cfgsync 0.1.0");

    cli::BuildCli(cli, Registry_, StorageManager_);

    try {
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
