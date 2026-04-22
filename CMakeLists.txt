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

project(cfgsync VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CFGSYNC_ENABLE_CLANG_TIDY "Enable clang-tidy for project sources" OFF)

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

cfgsync_use_cached_fetchcontent(CLI11 cli11-src)
cfgsync_use_cached_fetchcontent(fmt fmt-src)
cfgsync_use_cached_fetchcontent(spdlog spdlog-src)
cfgsync_use_cached_fetchcontent(nlohmann_json nlohmann_json-src)

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

add_executable(cfgsync
    src/main.cpp
    src/App.cpp
    src/cli/BuildCli.cpp
    src/core/Registry.cpp
    src/storage/StorageManager.cpp
    src/commands/InitCommand.cpp
    src/commands/AddCommand.cpp
    src/commands/RemoveCommand.cpp
    src/commands/ListCommand.cpp
    src/commands/BackupCommand.cpp
    src/commands/RestoreCommand.cpp
    src/utils/PathUtils.cpp
    src/utils/FileUtils.cpp
)

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
)

if(MSVC)
    target_compile_options(cfgsync PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync PRIVATE -Wall -Wextra -Wpedantic)
endif()
