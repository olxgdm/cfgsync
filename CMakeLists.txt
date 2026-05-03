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

enable_testing()

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
cfgsync_use_cached_fetchcontent(googletest googletest-src)

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

set(gtest_force_shared_crt ON CACHE BOOL "Use shared CRT for GoogleTest on Windows" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "Build GoogleMock" FORCE)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)

FetchContent_MakeAvailable(CLI11 fmt spdlog nlohmann_json googletest)

include(GoogleTest)

add_executable(cfgsync
    src/main.cpp
    src/App.cpp
    src/cli/BuildCli.cpp
    src/core/AppConfig.cpp
    src/core/Registry.cpp
    src/storage/StorageManager.cpp
    src/commands/InitCommand.cpp
    src/commands/AddCommand.cpp
    src/commands/RemoveCommand.cpp
    src/commands/ListCommand.cpp
    src/commands/BackupCommand.cpp
    src/commands/RestoreCommand.cpp
    src/utils/AppConfigPath.cpp
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

add_executable(cfgsync_app_config_tests
    tests/AppConfigTests.cpp
    tests/common/TestTempDirectory.cpp
    src/core/AppConfig.cpp
    src/utils/AppConfigPath.cpp
    src/utils/FileUtils.cpp
    src/utils/PathUtils.cpp
)

target_include_directories(cfgsync_app_config_tests PRIVATE src)

target_compile_options(cfgsync_app_config_tests BEFORE PRIVATE
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-iquote${googletest_SOURCE_DIR}/googletest/include>"
)

target_link_libraries(cfgsync_app_config_tests
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    GTest::gtest
)

if(MSVC)
    target_compile_options(cfgsync_app_config_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync_app_config_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(cfgsync_app_config_tests)

add_executable(cfgsync_registry_init_tests
    tests/RegistryInitTests.cpp
    tests/common/TestTempDirectory.cpp
    src/core/Registry.cpp
    src/utils/FileUtils.cpp
    src/utils/PathUtils.cpp
)

target_include_directories(cfgsync_registry_init_tests PRIVATE src)

target_compile_options(cfgsync_registry_init_tests BEFORE PRIVATE
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-iquote${googletest_SOURCE_DIR}/googletest/include>"
)

target_link_libraries(cfgsync_registry_init_tests
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    GTest::gtest
)

if(MSVC)
    target_compile_options(cfgsync_registry_init_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync_registry_init_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(cfgsync_registry_init_tests)

add_executable(cfgsync_registry_tests
    tests/RegistryTests.cpp
    tests/common/TestTempDirectory.cpp
    src/core/Registry.cpp
    src/utils/FileUtils.cpp
    src/utils/PathUtils.cpp
)

target_include_directories(cfgsync_registry_tests PRIVATE src)

target_compile_options(cfgsync_registry_tests BEFORE PRIVATE
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-iquote${googletest_SOURCE_DIR}/googletest/include>"
)

target_link_libraries(cfgsync_registry_tests
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    GTest::gtest
)

if(MSVC)
    target_compile_options(cfgsync_registry_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync_registry_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(cfgsync_registry_tests)

add_executable(cfgsync_path_file_utils_tests
    tests/PathFileUtilsTests.cpp
    tests/common/TestTempDirectory.cpp
    src/utils/FileUtils.cpp
    src/utils/PathUtils.cpp
)

target_include_directories(cfgsync_path_file_utils_tests PRIVATE src)

target_compile_options(cfgsync_path_file_utils_tests BEFORE PRIVATE
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-iquote${googletest_SOURCE_DIR}/googletest/include>"
)

target_link_libraries(cfgsync_path_file_utils_tests
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    GTest::gtest
)

if(MSVC)
    target_compile_options(cfgsync_path_file_utils_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync_path_file_utils_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(cfgsync_path_file_utils_tests)

add_executable(cfgsync_add_command_tests
    tests/AddCommandTests.cpp
    tests/common/TestTempDirectory.cpp
    src/commands/AddCommand.cpp
    src/core/Registry.cpp
    src/utils/FileUtils.cpp
    src/utils/PathUtils.cpp
)

target_include_directories(cfgsync_add_command_tests PRIVATE src)

target_compile_options(cfgsync_add_command_tests BEFORE PRIVATE
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-iquote${googletest_SOURCE_DIR}/googletest/include>"
)

target_link_libraries(cfgsync_add_command_tests
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    GTest::gtest
)

if(MSVC)
    target_compile_options(cfgsync_add_command_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync_add_command_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(cfgsync_add_command_tests)

add_executable(cfgsync_add_command_cli_tests
    tests/AddCommandCliTests.cpp
    tests/common/CliTestUtils.cpp
    tests/common/TestTempDirectory.cpp
    src/utils/FileUtils.cpp
    src/utils/PathUtils.cpp
)

target_include_directories(cfgsync_add_command_cli_tests PRIVATE src)

target_compile_options(cfgsync_add_command_cli_tests BEFORE PRIVATE
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-iquote${googletest_SOURCE_DIR}/googletest/include>"
)

target_link_libraries(cfgsync_add_command_cli_tests
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    GTest::gtest
)

if(MSVC)
    target_compile_options(cfgsync_add_command_cli_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync_add_command_cli_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_dependencies(cfgsync_add_command_cli_tests cfgsync)

gtest_discover_tests(cfgsync_add_command_cli_tests)

add_executable(cfgsync_list_command_tests
    tests/ListCommandTests.cpp
    tests/common/TestTempDirectory.cpp
    src/commands/ListCommand.cpp
    src/core/Registry.cpp
    src/utils/FileUtils.cpp
    src/utils/PathUtils.cpp
)

target_include_directories(cfgsync_list_command_tests PRIVATE src)

target_compile_options(cfgsync_list_command_tests BEFORE PRIVATE
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-iquote${googletest_SOURCE_DIR}/googletest/include>"
)

target_link_libraries(cfgsync_list_command_tests
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    GTest::gtest
)

if(MSVC)
    target_compile_options(cfgsync_list_command_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync_list_command_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(cfgsync_list_command_tests)

add_executable(cfgsync_list_command_cli_tests
    tests/ListCommandCliTests.cpp
    tests/common/CliTestUtils.cpp
    tests/common/TestTempDirectory.cpp
    src/utils/FileUtils.cpp
    src/utils/PathUtils.cpp
)

target_include_directories(cfgsync_list_command_cli_tests PRIVATE src)

target_compile_options(cfgsync_list_command_cli_tests BEFORE PRIVATE
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-iquote${googletest_SOURCE_DIR}/googletest/include>"
)

target_link_libraries(cfgsync_list_command_cli_tests
    PRIVATE
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    GTest::gtest
)

if(MSVC)
    target_compile_options(cfgsync_list_command_cli_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(cfgsync_list_command_cli_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_dependencies(cfgsync_list_command_cli_tests cfgsync)

gtest_discover_tests(cfgsync_list_command_cli_tests)
