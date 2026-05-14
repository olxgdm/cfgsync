cmake_minimum_required(VERSION 3.31)

# Enable modern CMake link-line deduplication and reordering.
# This prevents duplicate static libraries (for example fmt linked both
# directly and transitively through spdlog), which causes warnings with
# Apple/Xcode linker such as:
# ignoring duplicate libraries: 'libfmtd.a'
# CMP0156 allows platform-aware duplicate removal.
# CMP0179 defines which static library occurrence is kept.
# REORDER_FREELY gives CMake freedom to reorder dependencies safely.
if(POLICY CMP0156)
    cmake_policy(SET CMP0156 NEW)
endif()

if(POLICY CMP0179)
    cmake_policy(SET CMP0179 NEW)
endif()

set(CMAKE_LINK_LIBRARIES_STRATEGY REORDER_FREELY)

project(cfgsync VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CFGSYNC_ENABLE_CLANG_TIDY "Enable clang-tidy for project sources" OFF)
option(CFGSYNC_BUILD_TESTS "Build cfgsync GoogleTest test suite" ON)
option(CFGSYNC_ENABLE_COVERAGE "Enable Clang/LLVM coverage instrumentation for cfgsync targets" OFF)

include(FetchContent)

set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "Use external fmt" FORCE)

function(cfgsync_use_cached_fetchcontent name directory_name)
    string(TOUPPER "${name}" name_upper)

    foreach(dependency_root IN ITEMS "${CMAKE_BINARY_DIR}/_deps" "${CMAKE_SOURCE_DIR}/build/_deps")
        if(EXISTS "${dependency_root}/${directory_name}/CMakeLists.txt")
            set("FETCHCONTENT_SOURCE_DIR_${name_upper}" "${dependency_root}/${directory_name}" PARENT_SCOPE)
            break()
        endif()
    endforeach()
endfunction()

function(cfgsync_apply_coverage target_name)
    if(NOT CFGSYNC_ENABLE_COVERAGE)
        return()
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
        message(FATAL_ERROR "CFGSYNC_ENABLE_COVERAGE requires Clang or AppleClang")
    endif()

    target_compile_options(${target_name} PRIVATE
        -fprofile-instr-generate
        -fcoverage-mapping
        -O0
        -g
    )

    target_link_options(${target_name} PRIVATE
        -fprofile-instr-generate
    )
endfunction()

cfgsync_use_cached_fetchcontent(CLI11 cli11-src)
cfgsync_use_cached_fetchcontent(fmt fmt-src)
cfgsync_use_cached_fetchcontent(spdlog spdlog-src)
cfgsync_use_cached_fetchcontent(nlohmann_json nlohmann_json-src)
cfgsync_use_cached_fetchcontent(efsw efsw-src)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.5.0
)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 12.1.0
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)

FetchContent_MakeAvailable(CLI11 fmt spdlog nlohmann_json)

# efsw uses the global BUILD_SHARED_LIBS option. Keep it static so cfgsync
# does not need shared library runtime handling for the watcher backend.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build efsw static library" FORCE)
set(BUILD_TEST_APP OFF CACHE BOOL "Build efsw test application" FORCE)
set(EFSW_INSTALL OFF CACHE BOOL "Add efsw install targets" FORCE)

FetchContent_Declare(
    efsw
    GIT_REPOSITORY https://github.com/SpartanJ/efsw.git
    GIT_TAG 1.6.0
)

FetchContent_MakeAvailable(efsw)

if(TARGET efsw)
    set_target_properties(efsw PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

if(TARGET efsw-static)
    get_target_property(EFSW_STATIC_INCLUDE_DIRECTORIES efsw-static INTERFACE_INCLUDE_DIRECTORIES)
    if(EFSW_STATIC_INCLUDE_DIRECTORIES)
        set_target_properties(efsw-static PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "")
        target_include_directories(efsw-static SYSTEM INTERFACE ${EFSW_STATIC_INCLUDE_DIRECTORIES})
    endif()
endif()

add_executable(cfgsync
    src/main.cpp
    src/App.cpp
    src/cli/BuildCli.cpp
    src/core/AppConfig.cpp
    src/core/Registry.cpp
    src/storage/StorageManager.cpp
    src/diff/FileComparator.cpp
    src/diff/UnifiedDiff.cpp
    src/watch/EfswFileWatcher.cpp
    src/watch/WatchBackupProcessor.cpp
    src/commands/InitCommand.cpp
    src/commands/UseCommand.cpp
    src/commands/AddCommand.cpp
    src/commands/RemoveCommand.cpp
    src/commands/ListCommand.cpp
    src/commands/BackupCommand.cpp
    src/commands/StatusCommand.cpp
    src/commands/DiffCommand.cpp
    src/commands/WatchCommand.cpp
    src/commands/RestoreCommand.cpp
    src/utils/AppConfigPath.cpp
    src/utils/PathUtils.cpp
    src/utils/FileUtils.cpp
    src/utils/TerminalStyle.cpp
)

cfgsync_apply_coverage(cfgsync)

# Enable clang-tidy only for project sources.
# We intentionally do not enable it globally because that would also affect
# third-party dependencies pulled via FetchContent. This option allows local
# development and CI to turn static analysis on or off explicitly.
if(CFGSYNC_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)

    set_target_properties(cfgsync PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-p=${CMAKE_BINARY_DIR}"
    )
endif()

target_include_directories(cfgsync PRIVATE src)

target_link_libraries(cfgsync
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    efsw-static
)

if(MSVC)
    target_compile_options(cfgsync PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS cfgsync
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    README.md
    LICENSE
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

if(CFGSYNC_BUILD_TESTS)
    enable_testing()

    cfgsync_use_cached_fetchcontent(googletest googletest-src)

    set(gtest_force_shared_crt ON CACHE BOOL "Use shared CRT for GoogleTest on Windows" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "Build GoogleMock" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "Install GoogleTest" FORCE)

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )

    FetchContent_MakeAvailable(googletest)

    include("${CMAKE_CURRENT_LIST_DIR}/cmake/CfgsyncTesting.cmake")

    cfgsync_add_gtest_target(cfgsync_app_config_tests
        SOURCES
            tests/AppConfigTests.cpp
            tests/common/TestTempDirectory.cpp
            src/core/AppConfig.cpp
            src/utils/AppConfigPath.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_registry_init_tests
        SOURCES
            tests/RegistryInitTests.cpp
            tests/common/TestTempDirectory.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_registry_tests
        SOURCES
            tests/RegistryTests.cpp
            tests/common/TestTempDirectory.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_path_file_utils_tests
        SOURCES
            tests/PathFileUtilsTests.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
    )

    cfgsync_add_gtest_target(cfgsync_terminal_style_tests
        SOURCES
            tests/TerminalStyleTests.cpp
            src/utils/TerminalStyle.cpp
    )

    cfgsync_add_gtest_target(cfgsync_storage_manager_tests
        SOURCES
            tests/StorageManagerTests.cpp
            tests/common/TestTempDirectory.cpp
            src/storage/StorageManager.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_file_watcher_tests
        SOURCES
            tests/FileWatcherTests.cpp
    )

    cfgsync_add_gtest_target(cfgsync_watch_backup_processor_tests
        SOURCES
            tests/WatchBackupProcessorTests.cpp
            tests/common/TestTempDirectory.cpp
            src/watch/WatchBackupProcessor.cpp
            src/storage/StorageManager.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_file_comparator_tests
        SOURCES
            tests/FileComparatorTests.cpp
            tests/common/TestTempDirectory.cpp
            src/diff/FileComparator.cpp
            src/storage/StorageManager.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_unified_diff_tests
        SOURCES
            tests/UnifiedDiffTests.cpp
            tests/common/TestTempDirectory.cpp
            src/diff/UnifiedDiff.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
            src/utils/TerminalStyle.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_add_command_tests
        SOURCES
            tests/AddCommandTests.cpp
            tests/common/TestTempDirectory.cpp
            src/commands/AddCommand.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_use_command_tests
        SOURCES
            tests/UseCommandTests.cpp
            tests/common/TestTempDirectory.cpp
            src/commands/UseCommand.cpp
            src/storage/StorageManager.cpp
            src/core/AppConfig.cpp
            src/core/Registry.cpp
            src/utils/AppConfigPath.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_use_command_cli_tests CLI
        SOURCES
            tests/UseCommandCliTests.cpp
            tests/common/CliTestUtils.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_add_command_cli_tests CLI
        SOURCES
            tests/AddCommandCliTests.cpp
            tests/common/CliTestUtils.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_list_command_tests
        SOURCES
            tests/ListCommandTests.cpp
            tests/common/TestTempDirectory.cpp
            src/commands/ListCommand.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_list_command_cli_tests CLI
        SOURCES
            tests/ListCommandCliTests.cpp
            tests/common/CliTestUtils.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_remove_command_tests
        SOURCES
            tests/RemoveCommandTests.cpp
            tests/common/TestTempDirectory.cpp
            src/commands/RemoveCommand.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_remove_command_cli_tests CLI
        SOURCES
            tests/RemoveCommandCliTests.cpp
            tests/common/CliTestUtils.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_backup_command_tests
        SOURCES
            tests/BackupCommandTests.cpp
            tests/common/TestTempDirectory.cpp
            src/commands/BackupCommand.cpp
            src/storage/StorageManager.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_backup_command_cli_tests CLI
        SOURCES
            tests/BackupCommandCliTests.cpp
            tests/common/CliTestUtils.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_watch_command_tests
        SOURCES
            tests/WatchCommandTests.cpp
            tests/common/TestTempDirectory.cpp
            src/commands/WatchCommand.cpp
            src/watch/WatchBackupProcessor.cpp
            src/storage/StorageManager.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_watch_command_cli_tests CLI
        SOURCES
            tests/WatchCommandCliTests.cpp
            tests/common/CliTestUtils.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_status_command_tests
        SOURCES
            tests/StatusCommandTests.cpp
            tests/common/TestTempDirectory.cpp
            src/commands/StatusCommand.cpp
            src/diff/FileComparator.cpp
            src/storage/StorageManager.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_status_command_cli_tests CLI
        SOURCES
            tests/StatusCommandCliTests.cpp
            tests/common/CliTestUtils.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_diff_command_tests
        SOURCES
            tests/DiffCommandTests.cpp
            tests/common/TestTempDirectory.cpp
            src/commands/DiffCommand.cpp
            src/diff/FileComparator.cpp
            src/diff/UnifiedDiff.cpp
            src/storage/StorageManager.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
            src/utils/TerminalStyle.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_diff_command_cli_tests CLI
        SOURCES
            tests/DiffCommandCliTests.cpp
            tests/common/CliTestUtils.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
            src/utils/TerminalStyle.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_restore_command_tests
        SOURCES
            tests/RestoreCommandTests.cpp
            tests/common/TestTempDirectory.cpp
            src/commands/RestoreCommand.cpp
            src/storage/StorageManager.cpp
            src/core/Registry.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )

    cfgsync_add_gtest_target(cfgsync_restore_command_cli_tests CLI
        SOURCES
            tests/RestoreCommandCliTests.cpp
            tests/common/CliTestUtils.cpp
            tests/common/TestTempDirectory.cpp
            src/utils/FileUtils.cpp
            src/utils/PathUtils.cpp
        LIBRARIES
            nlohmann_json::nlohmann_json
    )
endif()
