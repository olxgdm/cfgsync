# cfgsync

`cfgsync` is a small cross-platform CLI utility for backing up and restoring text-based configuration files.

It is designed for a simple, local workflow: track important config files, save copies into a dedicated storage directory, and restore them later after a reinstall, migration, or accidental loss.

This project is not a Git replacement. It focuses on local configuration synchronization and recovery with a clean, minimal MVP.

## What It Does

- Tracks selected config files
- Stores backups in a readable local storage layout
- Restores saved files back to their original locations
- Keeps the workflow terminal-first and cross-platform

## MVP Commands

```bash
cfgsync init --storage <dir>
cfgsync add <file>
cfgsync remove <file>
cfgsync list
cfgsync backup
cfgsync restore --all
cfgsync restore <file>
```

## Scope

The current version targets ordinary files only and keeps the first iteration intentionally small.

Not part of the MVP:

- automatic file watching
- snapshot history
- diff support
- merge or conflict handling
- remote sync
- encryption
- directory tracking
- symlink tracking

## Platforms

`cfgsync` is being built with cross-platform support in mind for:

- Linux
- macOS
- Windows

## Tech

- C++20
- CMake
- CLI11
- nlohmann/json

## Status

The project is currently in early MVP development.
