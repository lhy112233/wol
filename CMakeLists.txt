cmake_minimum_required(VERSION 3.25)

project(wol VERSION 1.1 LANGUAGES CXX)

option(WOL_STATIC_LINK "Build wol as a fully static executable" OFF)
option(WOL_BUILD_TESTS "Build tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(WOL_FORMAT_FILES
    app/main.cpp
    include/wol/cli.hpp
    include/wol/config.hpp
    include/wol/help.hpp
    include/wol/learn.hpp
    include/wol/network.hpp
    include/wol/types.hpp
    src/cli.cpp
    src/config.cpp
    src/help.cpp
    src/learn.cpp
    src/network.cpp
    src/toml.cpp
    src/types.cpp
    tests/wol_tests.cpp
)

set(WOL_FORMAT_PATHS)
foreach(file IN LISTS WOL_FORMAT_FILES)
    list(APPEND WOL_FORMAT_PATHS "${CMAKE_SOURCE_DIR}/${file}")
endforeach()

set(WOL_TIDY_FILES
    app/main.cpp
    src/cli.cpp
    src/config.cpp
    src/help.cpp
    src/learn.cpp
    src/network.cpp
    src/toml.cpp
    src/types.cpp
    tests/wol_tests.cpp
)

set(WOL_TIDY_PATHS)
foreach(file IN LISTS WOL_TIDY_FILES)
    list(APPEND WOL_TIDY_PATHS "${CMAKE_SOURCE_DIR}/${file}")
endforeach()

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${WOL_FORMAT_PATHS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking C++ formatting with clang-format"
        VERBATIM
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${WOL_FORMAT_PATHS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting C++ sources with clang-format"
        VERBATIM
    )
else()
    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "clang-format was not found"
    )
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "clang-format was not found"
    )
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p=${CMAKE_BINARY_DIR}
            --quiet
            --warnings-as-errors=*
            ${WOL_TIDY_PATHS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy with C++ Core Guidelines checks"
        VERBATIM
    )
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "clang-tidy was not found"
    )
endif()

if(WOL_STATIC_LINK)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_LINK_OPTIONS -static -static-libgcc -static-libstdc++)
    check_cxx_source_compiles("int main() { return 0; }" WOL_CAN_LINK_STATIC_RUNTIME)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
    if(NOT WOL_CAN_LINK_STATIC_RUNTIME)
        message(FATAL_ERROR
            "The selected compiler cannot link a fully static C++ executable. "
            "Install static runtime packages such as Fedora glibc-static/libstdc++-static "
            "or the Debian/Ubuntu equivalents, then reconfigure.")
    endif()
endif()

add_library(wol_core
    src/cli.cpp
    src/config.cpp
    src/help.cpp
    src/learn.cpp
    src/network.cpp
    src/toml.cpp
    src/types.cpp
)

target_include_directories(wol_core PUBLIC include)
target_compile_definitions(wol_core PRIVATE WOL_VERSION="${PROJECT_VERSION}")
target_compile_options(wol_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(wol app/main.cpp)
target_link_libraries(wol PRIVATE wol_core)
target_compile_options(wol PRIVATE -Wall -Wextra -Wpedantic)

if(WOL_STATIC_LINK)
    target_link_options(wol PRIVATE -static -static-libgcc -static-libstdc++)
endif()

if(WOL_BUILD_TESTS)
    enable_testing()
    include(GoogleTest)
    find_package(GTest REQUIRED)
    add_executable(wol_tests tests/wol_tests.cpp)
    target_link_libraries(wol_tests PRIVATE wol_core GTest::gtest_main)
    target_compile_options(wol_tests PRIVATE -Wall -Wextra -Wpedantic)
    gtest_discover_tests(wol_tests)
    add_test(NAME cli_help COMMAND wol --help)
    add_test(NAME cli_version COMMAND wol --version)
    add_test(NAME cli_check_config COMMAND wol --config ${CMAKE_SOURCE_DIR}/wol.toml --check-config)
    add_test(NAME cli_check_config_json COMMAND wol --config ${CMAKE_SOURCE_DIR}/wol.toml --check-config --json)
    add_test(NAME cli_print_config_path COMMAND wol --print-config-path)
    add_test(NAME cli_dry_run_default COMMAND wol --config ${CMAKE_SOURCE_DIR}/wol.toml --dry-run)
    add_test(NAME cli_dry_run_default_json COMMAND wol --config ${CMAKE_SOURCE_DIR}/wol.toml --dry-run --json)
    add_test(NAME cli_dry_run_named COMMAND wol --config ${CMAKE_SOURCE_DIR}/wol.toml --dry-run desktop)
    add_test(NAME cli_list COMMAND wol --config ${CMAKE_SOURCE_DIR}/wol.toml --list)
    add_test(NAME cli_list_json COMMAND wol --config ${CMAKE_SOURCE_DIR}/wol.toml --list --json)
    add_test(NAME cli_unknown_target COMMAND wol --config ${CMAKE_SOURCE_DIR}/wol.toml unknown)
    set_tests_properties(cli_unknown_target PROPERTIES WILL_FAIL TRUE)
    add_test(NAME cli_missing_config COMMAND wol --config ${CMAKE_SOURCE_DIR}/does-not-exist.toml --check-config)
    set_tests_properties(cli_missing_config PROPERTIES WILL_FAIL TRUE)
    add_test(NAME cli_business_outputs
        COMMAND ${CMAKE_COMMAND}
            -DWOL_BINARY=$<TARGET_FILE:wol>
            -DSAMPLE_CONFIG=${CMAKE_SOURCE_DIR}/wol.toml
            -DMISSING_CONFIG=${CMAKE_SOURCE_DIR}/does-not-exist.toml
            -DJSON_TEMP_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-json
            -P ${CMAKE_SOURCE_DIR}/tests/CliBehavior.cmake
    )
endif()

install(TARGETS wol RUNTIME DESTINATION .)
install(FILES wol.toml DESTINATION .)

add_custom_target(package_wol
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_BINARY_DIR}/package
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/package
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:wol> ${CMAKE_BINARY_DIR}/package/wol
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/wol.toml ${CMAKE_BINARY_DIR}/package/wol.toml
    COMMAND ${CMAKE_COMMAND}
        -DWOL_BINARY=$<TARGET_FILE:wol>
        -P ${CMAKE_SOURCE_DIR}/cmake/VerifyStatic.cmake
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/dist
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_BINARY_DIR}/package
        ${CMAKE_COMMAND} -E tar czf ${CMAKE_SOURCE_DIR}/dist/wol-linux-x86_64.tar.gz
            --format=gnutar
            -- wol wol.toml
    DEPENDS wol
    VERBATIM
)
