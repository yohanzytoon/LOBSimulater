cmake_minimum_required(VERSION 3.18)
project(lob-backtester LANGUAGES CXX)

# Options to enable tests, Python bindings, and sanitizers.  These can be
# toggled when invoking cmake.  Sanitizers are on by default for debug
# builds to help catch undefined behaviour.
option(LOB_ENABLE_SANITIZERS "Enable ASan/UBSan" ON)
option(LOB_ENABLE_TIDY       "Enable clang-tidy in build" OFF)
option(LOB_BUILD_BINDINGS    "Build pybind11 Python module" ON)
option(LOB_BUILD_TESTS       "Build tests" ON)
option(LOB_BUILD_BENCH       "Build benchmarks" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Treat warnings as errors and enable a fairly strict warning set on
# non-MSVC compilers.
if (MSVC)
  add_compile_options(/W4 /permissive- /EHsc)
else()
  add_compile_options(
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
    -Werror=return-type
  )
endif()

# AddressSanitizer and UndefinedBehaviourSanitizer help detect bugs
if (LOB_ENABLE_SANITIZERS AND NOT MSVC)
  add_link_options(-fsanitize=address,undefined)
  add_compile_options(-fsanitize=address,undefined)
endif()

# Enable clang-tidy if requested and available.  This will emit
# diagnostics at build time but will not fail the build unless
# explicitly configured in .clang-tidy.
if (LOB_ENABLE_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if (CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()
endif()

# Main library consisting of order book, backtester, signals and metrics.
add_library(lob STATIC
  src/order_book.cpp
  src/backtester.cpp
  src/signals.cpp
  src/metrics.cpp
)
target_include_directories(lob PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(lob PUBLIC LOB_VERSION="0.1.0")

# Example driver executable
add_executable(lob_main src/main.cpp)
target_link_libraries(lob_main PRIVATE lob)

# Strategy examples
add_executable(example_market_maker examples/market_maker.cpp)
target_link_libraries(example_market_maker PRIVATE lob)
add_executable(example_momentum examples/momentum.cpp)
target_link_libraries(example_momentum PRIVATE lob)
add_executable(example_signal_exec examples/signal_execution.cpp)
target_link_libraries(example_signal_exec PRIVATE lob)

# Unit and integration tests using Catch2.  Catch2 is downloaded via
# FetchContent which automatically handles caching across builds.
if (LOB_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
  )
  FetchContent_MakeAvailable(Catch2)
  add_executable(unit_tests
    tests/test_order_book.cpp
    tests/test_signals.cpp
    tests/test_backtester.cpp
  )
  target_link_libraries(unit_tests PRIVATE lob Catch2::Catch2WithMain)
  include(CTest)
  add_test(NAME unit_tests COMMAND unit_tests)
endif()

# Benchmarks to measure throughput/latency.  These can be built by
# enabling LOB_BUILD_BENCH.
if (LOB_BUILD_BENCH)
  add_executable(bench_order_book benchmarks/bench_order_book.cpp)
  target_link_libraries(bench_order_book PRIVATE lob)
endif()

# Build Python bindings if requested.  The bindings are located in
# bindings/ and rely on pybind11.  The subdirectory contains its own
# CMakeLists.txt.
if (LOB_BUILD_BINDINGS)
  add_subdirectory(bindings)
endif()

# Documentation generation with Doxygen.  Running `make docs` will
# produce HTML documentation in docs/build.
find_package(Doxygen QUIET)
if (DOXYGEN_FOUND)
  add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
  )
endif()