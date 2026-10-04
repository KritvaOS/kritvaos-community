cmake_minimum_required(VERSION 3.24)

project(Kritva
    VERSION 0.1.0
    DESCRIPTION "Open Robotic Computing Platform"
    LANGUAGES CXX
)

# ------------------------------------------------------------------------------
# Project-wide configuration
# ------------------------------------------------------------------------------

option(KRITVA_BUILD_TESTS
    "Build Kritva tests"
    ON
)

option(KRITVA_BUILD_EXAMPLES
    "Build Kritva examples"
    ON
)

option(KRITVA_BUILD_TOOLS
    "Build Kritva development tools"
    ON
)

option(KRITVA_BUILD_SIM
    "Build Kritva simulation components"
    ON
)

option(KRITVA_BUILD_SOC
    "Enable Kritva SoC development components"
    OFF
)

option(KRITVA_ENABLE_WARNINGS
    "Enable compiler warnings"
    ON
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
# Project-wide compiler configuration
# ------------------------------------------------------------------------------

if(KRITVA_ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(
            -Wall
            -Wextra
            -Wpedantic
        )
    elseif(MSVC)
        add_compile_options(/W4)
    endif()
endif()

# ------------------------------------------------------------------------------
# Kritva Core
# ------------------------------------------------------------------------------

add_subdirectory(core)

# ------------------------------------------------------------------------------
# Kritva Software Platform
# ------------------------------------------------------------------------------

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sense/CMakeLists.txt")
    add_subdirectory(sense)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/mind/CMakeLists.txt")
    add_subdirectory(mind)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/motion/CMakeLists.txt")
    add_subdirectory(motion)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/skill/CMakeLists.txt")
    add_subdirectory(skill)
endif()

if(KRITVA_BUILD_SIM AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sim/CMakeLists.txt")
    add_subdirectory(sim)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sdk/CMakeLists.txt")
    add_subdirectory(sdk)
endif()

# ------------------------------------------------------------------------------
# KritvaOS Application Runtime (KOS-I2)
# ------------------------------------------------------------------------------

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/CMakeLists.txt")
    add_subdirectory(runtime)
endif()

# ------------------------------------------------------------------------------
# Hardware and Drivers
# ------------------------------------------------------------------------------

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/hardware/CMakeLists.txt")
    add_subdirectory(hardware)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/drivers/CMakeLists.txt")
    add_subdirectory(drivers)
endif()

# ------------------------------------------------------------------------------
# SoC Platforms
# ------------------------------------------------------------------------------

if(KRITVA_BUILD_SOC AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/soc/CMakeLists.txt")
    add_subdirectory(soc)
endif()

# ------------------------------------------------------------------------------
# Examples
# ------------------------------------------------------------------------------

if(KRITVA_BUILD_EXAMPLES AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
endif()

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------

if(KRITVA_BUILD_TESTS)
    enable_testing()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        add_subdirectory(tests)
    endif()
endif()

# ------------------------------------------------------------------------------
# Tools
# ------------------------------------------------------------------------------

if(KRITVA_BUILD_TOOLS AND
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt")
    add_subdirectory(tools)
endif()
