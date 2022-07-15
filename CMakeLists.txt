cmake_minimum_required(VERSION 3.16)
project(matthewtolman_site)

set(CMAKE_CXX_STANDARD 20)

# Dependency Setup
add_subdirectory(generator/deps/Mustache-4.1)
add_subdirectory(generator/deps/range-v3-0.12.0)

# VERSION SETUP
find_program(NODE node REQUIRED)
find_program(GIT_EXECUTABLE git REQUIRED)
add_custom_target(version_h BYPRODUCTS "${CMAKE_BINARY_DIR}/version-dummy.h" "${CMAKE_CURRENT_SOURCE_DIR}/generator/version.h"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${NODE} ${CMAKE_CURRENT_SOURCE_DIR}/setup-version.js ${GIT_EXECUTABLE})

# GENERATOR SETUP
set(SRCS
        generator/src/parse.cpp
        generator/src/cli_args.cpp
        "${CMAKE_BINARY_DIR}/version-dummy.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/generator/version.h"
        generator/src/pipeline.cpp generator/src/files.cpp generator/src/files.h generator/src/str_utils.cpp generator/src/str_utils.hpp generator/src/eval_engine.cpp generator/src/eval_engine.hpp)
set(TEST_SRCS generator/test/parse.cpp generator/test/cli_args.cpp generator/test/files.cpp generator/test/str_utils.cpp generator/test/eval_engine.cpp)
add_executable(generator ${SRCS} generator/main.cpp)
add_executable(generator_tests generator/test.cpp ${SRCS} ${TEST_SRCS})

target_link_libraries(generator PUBLIC range-v3)
target_link_libraries(generator_tests PUBLIC range-v3)

target_include_directories(generator PUBLIC generator/deps generator/src)
target_include_directories(generator_tests PUBLIC generator/deps/test generator/src generator/deps)

enable_testing()
add_test(NAME generator_tests COMMAND generator_tests)

# SITE SETUP
add_custom_target(static_site BYPRODUCTS
        "${CMAKE_BINARY_DIR}/dummy-file.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/dest/index.html"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND generator "${CMAKE_CURRENT_SOURCE_DIR}/blog" "${CMAKE_CURRENT_SOURCE_DIR}/dest")
