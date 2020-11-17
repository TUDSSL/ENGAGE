# Author: TU Delft Sustainable Systems Laboratory
# License: MIT License

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

set(CMAKE_TOOLCHAIN_FILE "config/apollo-toolchain.cmake")

project(gby LANGUAGES C ASM)

# Prefix path to install executables
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})

if(NOT (EXISTS "${CMAKE_SOURCE_DIR}/external/F746_Gameboy" AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/external/F746_Gameboy"))
message("Downloading emulator and applying patches. Can take a while.")
execute_process(COMMAND "${CMAKE_SOURCE_DIR}/scripts/getEmulator.sh"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

add_subdirectory(apps)

# Only generate build files for docs if explicitely set BUILD_DOCS as a
# command-line argument, as building docs requires Doxygen, Sphinx and other
# Sphinx-related dependencies
if(BUILD_DOCS)
    add_subdirectory(docs EXCLUDE_FROM_ALL)
endif()

