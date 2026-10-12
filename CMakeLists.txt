cmake_minimum_required(VERSION 3.25)
project(VCSNX VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(VCSNX_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/vcs-recomp")
set(VCSNX_PROFILE_DIR "${VCSNX_VENDOR_DIR}/profiles/vcs")
set(VCSNX_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/switch")
set(VCSNX_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated" CACHE PATH
    "Regenerated AOT units (tools/regen_generated.sh); never committed")

if(CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    set(VCSNX_TARGET_SWITCH TRUE)
else()
    set(VCSNX_TARGET_SWITCH FALSE)
endif()

# Only the framework comes from the vendor build: its profile CMake picks its own windows and
# platform layers per OS, so VCSNX builds the profile itself.
set(PSPRECOMP_PROFILE "" CACHE STRING "Unused by VCSNX; the profile is built by cmake/vcsnx_game.cmake" FORCE)
if(VCSNX_TARGET_SWITCH)
    set(PSPRECOMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${VCSNX_VENDOR_DIR}" vendor EXCLUDE_FROM_ALL)
else()
    enable_testing()
    add_subdirectory("${VCSNX_VENDOR_DIR}" vendor)
    include(cmake/vcsnx_host_tools.cmake)
    include(cmake/vcsnx_tests.cmake)
endif()
include(cmake/vcsnx_vulkan.cmake)
include(cmake/vcsnx_game.cmake)
if(VCSNX_TARGET_SWITCH)
    include(cmake/vcsnx_switch.cmake)
    include(cmake/vcsnx_vkprobe.cmake)
endif()
