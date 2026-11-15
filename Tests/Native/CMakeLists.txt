# Standalone build of the PadelCore simulation code with its native tests.
# Runs on any machine with CMake and a C++20 compiler, no Unreal Engine needed.
#
#   cmake -S Tests/Native -B build/native && cmake --build build/native -j && ctest --test-dir build/native --output-on-failure
cmake_minimum_required(VERSION 3.20)
project(PadelCoreNativeTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(CORE_DIR ${REPO_ROOT}/Source/PadelCore)

# Every PadelCore .cpp except the Unreal module boilerplate and Unreal-only tests.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS ${CORE_DIR}/Private/*.cpp)
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/PadelCoreModule\\.cpp$")
list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/Private/Tests/.*")

# The game module's engine-free models (UI and presentation maths): plain C++ with no
# Unreal headers, so they're tested here too. Only */Model/ folders, never UObject code.
set(GAME_DIR ${REPO_ROOT}/Source/Padel)
file(GLOB_RECURSE MODEL_SOURCES CONFIGURE_DEPENDS ${GAME_DIR}/Private/*/Model/*.cpp)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(padel_core_tests ${CORE_SOURCES} ${MODEL_SOURCES} ${TEST_SOURCES})
target_include_directories(padel_core_tests PRIVATE ${CORE_DIR}/Public ${GAME_DIR}/Public ${REPO_ROOT}/ThirdParty/doctest)
# The tests read the real tuning files under Data/.
target_compile_definitions(padel_core_tests PRIVATE PADEL_REPO_ROOT="${REPO_ROOT}")

if(MSVC)
  target_compile_options(padel_core_tests PRIVATE /W4 /WX)
else()
  target_compile_options(padel_core_tests PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
endif()

enable_testing()
add_test(NAME padel_core_tests COMMAND padel_core_tests)
