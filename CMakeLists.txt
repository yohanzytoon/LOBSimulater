cmake_minimum_required(VERSION 3.20)

# Set project name and version
project(
    LOBBacktester
    VERSION 1.0.0
    DESCRIPTION "High-performance Limit Order Book Simulator & Event-Driven Backtester"
    LANGUAGES CXX
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Options
option(LOB_BUILD_TESTS "Build unit tests" ON)
option(LOB_BUILD_BENCHMARKS "Build benchmarks" ON)
option(LOB_BUILD_EXAMPLES "Build example strategies" ON)
option(LOB_BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(LOB_ENABLE_COVERAGE "Enable code coverage reporting" OFF)
option(LOB_ENABLE_SANITIZERS "Enable sanitizers" OFF)
option(LOB_ENABLE_STATIC_ANALYSIS "Enable static analysis tools" OFF)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /Wall")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
endif()

# Find ccache for faster builds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Enable interprocedural optimization if available
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
if(ipo_supported)
    message(STATUS "IPO enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    message(STATUS "IPO not supported: ${ipo_error}")
endif()

# Dependencies
include(FetchContent)

# GoogleTest for unit testing
if(LOB_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

# Google Benchmark for performance testing
if(LOB_BUILD_BENCHMARKS)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
        GIT_SHALLOW TRUE
    )
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googlebenchmark)
endif()

# pybind11 for Python bindings
if(LOB_BUILD_PYTHON_BINDINGS)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pybind11)
endif()

# Fast hash map implementation
FetchContent_Declare(
    robin-hood-hashing
    GIT_REPOSITORY https://github.com/martinus/robin-hood-hashing.git
    GIT_TAG 3.11.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(robin-hood-hashing)

# Create the main LOB library
add_library(LOBCore
    src/order_book.cpp
    src/backtester.cpp
    src/signals.cpp
    src/metrics.cpp
)

add_library(LOB::Core ALIAS LOBCore)

target_include_directories(LOBCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(LOBCore
    PUBLIC
        robin_hood::robin_hood
)

target_compile_features(LOBCore PUBLIC cxx_std_17)

# Compiler warnings
if(MSVC)
    target_compile_options(LOBCore PRIVATE /W4)
else()
    target_compile_options(LOBCore PRIVATE 
        -Wall -Wextra -Wpedantic -Werror
        -Wno-unused-parameter
    )
endif()

# Code coverage
if(LOB_ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(LOBCore PRIVATE --coverage)
    target_link_libraries(LOBCore PRIVATE --coverage)
endif()

# Sanitizers
if(LOB_ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(LOBCore PRIVATE 
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_libraries(LOBCore PRIVATE 
            -fsanitize=address,undefined
        )
    endif()
endif()

# Example executable
add_executable(LOBExample src/main.cpp)
target_link_libraries(LOBExample PRIVATE LOB::Core)

# Subdirectories
if(LOB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LOB_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(LOB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LOB_BUILD_PYTHON_BINDINGS)
    add_subdirectory(bindings)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS LOBCore
    EXPORT LOBTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/lob
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT LOBTargets
    FILE LOBTargets.cmake
    NAMESPACE LOB::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LOB
)

# Create and install package config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LOBConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/LOBConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LOB
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/LOBConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/LOBConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/LOBConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LOB
)

# Documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in 
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(docs
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "==================== Build Configuration ====================")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "Options:")
message(STATUS "  Tests: ${LOB_BUILD_TESTS}")
message(STATUS "  Benchmarks: ${LOB_BUILD_BENCHMARKS}")
message(STATUS "  Examples: ${LOB_BUILD_EXAMPLES}")
message(STATUS "  Python bindings: ${LOB_BUILD_PYTHON_BINDINGS}")
message(STATUS "  Code coverage: ${LOB_ENABLE_COVERAGE}")
message(STATUS "  Sanitizers: ${LOB_ENABLE_SANITIZERS}")
message(STATUS "  Static analysis: ${LOB_ENABLE_STATIC_ANALYSIS}")
message(STATUS "")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "==============================================================")
message(STATUS "")