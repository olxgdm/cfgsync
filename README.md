![cfgsync preview](img/cfgsync-preview-rm.png)

# cfgsync

[![CI](https://github.com/olxgdm/cfgsync/actions/workflows/cmake-multi-platform.yml/badge.svg?branch=main)](https://github.com/olxgdm/cfgsync/actions/workflows/cmake-multi-platform.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)
![CMake](https://img.shields.io/badge/CMake-3.31%2B-064F8C.svg)

`cfgsync` is a small cross-platform CLI utility for backing up and restoring text-based configuration files.

It is designed for a simple local workflow: track important config files, save copies into a dedicated storage directory, and restore them later after a reinstall, migration, or accidental loss.

This project is not a Git replacement. It focuses on local configuration synchronization and recovery with a clean, minimal MVP.

## Status

The v0.1.1 MVP workflow is implemented:

- initialize a cfgsync storage directory
- persist the active storage root in a user-level app config
- add and remove ordinary files from the registry
- list tracked files
- back up tracked files into storage
- watch tracked files and automatically back up changes
- adopt an existing storage directory on a fresh system
- restore one tracked file or all tracked files

The current version intentionally supports ordinary files only. Directory tracking, symlink tracking, snapshots, remote sync, and encryption are outside the v0 scope.

## Prerequisites

- CMake 3.31 or newer
- A C++20-capable compiler
- Git and network access during the first CMake configure, because dependencies are fetched with CMake `FetchContent`

The project fetches these dependencies during configuration:

- CLI11
- fmt
- spdlog
- nlohmann/json
- efsw
- GoogleTest, when `CFGSYNC_BUILD_TESTS` is enabled

## Build

Configure and build from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On single-config generators, such as Unix Makefiles or Ninja, the executable is usually written to:

```text
build/cfgsync
```

On multi-config generators, such as Visual Studio, the executable is usually written under the selected configuration directory, for example:

```text
build/Release/cfgsync.exe
```

Tests are enabled by default. To configure a smaller build without the GoogleTest suite:

```bash
cmake -S . -B build-no-tests -DCFGSYNC_BUILD_TESTS=OFF
cmake --build build-no-tests
```

Optional clang-tidy checks can be enabled with:

```bash
cmake -S . -B build -DCFGSYNC_ENABLE_CLANG_TIDY=ON
```

## Test

Build the project and run CTest:

```bash
cmake -S . -B build -DCFGSYNC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

The test suite uses temporary directories and is designed not to write to the real user home directory or real cfgsync app config.

## Quick Start

The examples below assume `cfgsync` is available on your `PATH`. If it is not, run the executable from your build directory instead.

```bash
cfgsync init --storage ~/cfgsync-store
cfgsync add ~/.gitconfig
cfgsync list
cfgsync backup
cfgsync watch
cfgsync restore ~/.gitconfig
cfgsync restore --all
cfgsync remove ~/.gitconfig
```

Typical flow:

1. Run `cfgsync init --storage <dir>` once to create the storage directory and record it as the active storage root.
2. Run `cfgsync add <file>` for each ordinary config file you want to track.
3. Run `cfgsync backup` whenever you want to copy the current tracked files into storage.
4. Optionally run `cfgsync watch` to keep backing up tracked files as they change until you stop it.
5. Run `cfgsync restore <file>` to restore one tracked file, or `cfgsync restore --all` to restore every tracked file.
6. Run `cfgsync remove <file>` when a file should no longer be tracked.

Fresh-system restore flow:

```bash
cfgsync use --storage ~/cfgsync-store
cfgsync list
cfgsync restore --all
```

Use `cfgsync use --storage <dir>` when a cfgsync storage directory already exists, for example after reinstalling an operating system or copying storage onto a new machine. The command validates the storage, records it as active in the app config, and does not restore files by itself.

After `use`, inspect tracked paths with `cfgsync list`, then restore everything with `cfgsync restore --all` or restore one tracked file with:

```bash
cfgsync restore ~/.gitconfig
```

## Commands

### `cfgsync init --storage <dir>`

Initializes cfgsync storage at the given directory.

This creates:

- the storage root directory, if needed
- `registry.json`
- `files/`
- a user-level cfgsync app config that records the active storage root

After `init`, later commands do not need `--storage`. They load the active storage root from the app config.

The app config is stored at:

- Linux/macOS: `$HOME/.config/cfgsync/config.json`
- Windows: `%APPDATA%/cfgsync/config.json`

Running `init` again against an existing valid storage root preserves the registry and ensures the storage layout exists.

### `cfgsync use --storage <dir>`

Uses an existing cfgsync storage directory as the active storage root.

This validates:

- the storage directory exists
- `registry.json` exists
- the registry format is valid and supported

If the storage directory was copied or moved, `use` updates the registry `storage_root` to the selected storage path and saves the app config. It also ensures the storage `files/` directory exists.

The command does not restore files. Run `cfgsync list` to inspect tracked original paths, then run `cfgsync restore --all` or `cfgsync restore <file>` when you are ready to overwrite destination files.

In v0, restore uses the exact original paths recorded in the registry. `cfgsync use` does not remap paths across different usernames, home directories, or operating systems.

### `cfgsync add <file>`

Registers an existing ordinary file for tracking.

The path is expanded for `~`, normalized, and stored in the registry. Directories, symlinks, missing paths, and special files are rejected in v0.

Adding an already tracked file leaves the registry unchanged.

### `cfgsync remove <file>`

Removes a tracked file from the registry.

This does not delete the original file and does not delete any previously stored backup copy.

### `cfgsync list`

Prints tracked original file paths, one per line.

If no files are tracked, it prints:

```text
No files tracked.
```

### `cfgsync backup`

Copies every tracked file from its original location into the storage `files/` tree.

If one tracked file cannot be backed up, cfgsync reports that file, continues with the remaining entries, and exits with a failure after the batch finishes.

### `cfgsync diff <file>`

Shows a unified text diff between a tracked file's stored backup and its current original file.

The stored backup is shown as the old side and the current original file is shown as the new side. Identical files produce no output and still exit successfully.

The file path is normalized before lookup. The command fails if the file is not tracked, if the original file is missing, if the tracked file has no stored backup yet, or if the file contains unsupported binary content.

### `cfgsync watch`

Runs in the foreground and watches tracked original files for changes.

The command watches tracked files' parent directories, ignores untracked files in those directories, and backs up a tracked file when it is added, modified, or moved into place. Duplicate events for the same file are debounced with a short delay. The command does not perform an initial backup on startup.

If a tracked file is deleted, becomes non-ordinary, or cannot be backed up, cfgsync reports a warning and keeps watching. Press Ctrl+C to stop watching.

### `cfgsync restore --all`

Restores every tracked file from storage back to its original location.

Parent directories are created before files are restored. Existing destination files are overwritten.

If one tracked file cannot be restored, cfgsync reports that file, continues with the remaining entries, and exits with a failure after the batch finishes.

### `cfgsync restore <file>`

Restores one tracked file from storage back to its original location.

The file path is normalized before lookup. The command fails if the file is not tracked or if no stored backup exists.

## Storage Layout

The storage root is intentionally readable in v0:

```text
storage/
  registry.json
  files/
    home/
      user/
        .gitconfig
        .config/
          nvim/
            init.lua
```

For POSIX paths, the absolute path is mapped under `files/` without the leading slash:

```text
/home/user/.gitconfig -> files/home/user/.gitconfig
```

For Windows-style drive paths, the drive letter becomes a directory segment:

```text
C:\Users\Oleksii\.gitconfig -> files/C/Users/Oleksii/.gitconfig
```

The layout favors readability over hashing for v0.

## Registry Format

The registry is stored as JSON at `<storage>/registry.json`.

Example:

```json
{
    "version": 1,
    "storage_root": "/absolute/path/to/storage",
    "tracked_files": [
        {
            "original_path": "/home/user/.gitconfig",
            "stored_relative_path": "files/home/user/.gitconfig"
        }
    ]
}
```

The registry records:

- the registry format version
- the storage root associated with the registry
- each tracked original path
- the relative path where that file is stored under the storage root

Users normally do not need to edit this file manually, but it is kept readable for inspection and future migration.

## v0 Limitations

The v0 scope is intentionally small:

- ordinary files only
- no directory tracking
- no symlink tracking
- no special file handling
- no snapshots or history
- no diff support
- no merge or conflict resolution
- no original-path remapping across users, home directories, or operating systems
- no remote sync
- no encryption
- no automatic file watching
- no packaging or installer flow yet

## Development Notes

The codebase is organized around a small layered design:

- `src/cli/` defines command-line structure and argument parsing
- `src/commands/` contains thin command handlers
- `src/core/` contains registry and app configuration logic
- `src/storage/` maps tracked entries to storage paths and performs backup/restore copies
- `src/utils/` contains path, filesystem, logging, and app config path helpers
- `tests/` contains focused GoogleTest coverage and CLI-level tests

The MVP prioritizes correctness, clear errors, cross-platform path handling through `std::filesystem`, and a maintainable structure before advanced features.
