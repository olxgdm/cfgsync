include(GoogleTest)

function(cfgsync_apply_project_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(cfgsync_add_gtest_target target_name)
    set(options CLI)
    set(one_value_args)
    set(multi_value_args SOURCES LIBRARIES)
    cmake_parse_arguments(CFGSYNC_TEST "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    add_executable(${target_name}
        ${CFGSYNC_TEST_SOURCES}
    )

    target_include_directories(${target_name} PRIVATE src)
    target_include_directories(${target_name} SYSTEM PRIVATE
        "${googletest_SOURCE_DIR}/googletest/include"
    )

    target_link_libraries(${target_name}
        PRIVATE
        fmt::fmt
        spdlog::spdlog
        GTest::gtest
        ${CFGSYNC_TEST_LIBRARIES}
    )

    cfgsync_apply_project_warnings(${target_name})
    cfgsync_apply_coverage(${target_name})

    if(CFGSYNC_TEST_CLI)
        add_dependencies(${target_name} cfgsync)
    endif()

    gtest_discover_tests(${target_name}
        DISCOVERY_TIMEOUT 30
    )
endfunction()
