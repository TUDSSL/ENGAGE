# Author: Jasper de Winkel

project(memtracker VERSION 1.0.0 LANGUAGES C ASM)

set(DEPENDENCIES
    external/CMSIS/ARM/Include
    external/CMSIS/AmbiqMicro/Include
    external/utils
    external/mcu/apollo3/
    external/mcu/apollo3/hal/
    config/
    platforms/
    libs/buttons
    libs/display
    libs/emulator
    libs/memtracker
    libs/jit/
    libs/cartridge/
    external/F746_Gameboy/inc
    )

# Source files of libraries and externals
foreach(dep ${DEPENDENCIES})
    list(APPEND DEP_SOURCES "${CMAKE_SOURCE_DIR}/${dep}/*.[cs]")
endforeach()

list(APPEND DEP_SOURCES 
"${CMAKE_SOURCE_DIR}/external/F746_Gameboy/src/z80_ub.c"
"${CMAKE_SOURCE_DIR}/external/F746_Gameboy/src/z80_opcode.c"
"${CMAKE_SOURCE_DIR}/external/F746_Gameboy/src/z80_opcode_cb.c"
"${CMAKE_SOURCE_DIR}/external/F746_Gameboy/src/gameboy_ub.c"
"${CMAKE_SOURCE_DIR}/external/F746_Gameboy/roms/Boot_Rom.c"
)

# List source files to be compiled
file(GLOB SOURCES
    "${PROJECT_SOURCE_DIR}/*.[cs]"
    ${DEP_SOURCES}
    )

# Add executable target
add_executable(${PROJECT_NAME} ${SOURCES})

# Change target suffix to have <name>.out
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ${OUTPUT_SUFFIX})

# Defines (-D)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE HWREV=1
    PRIVATE AM_UTIL_FAULTISR_PRINT
)

# Compiler options for this project
target_compile_options(${PROJECT_NAME}
    PUBLIC -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    PRIVATE -O0
    PRIVATE -g -gdwarf-3
    PRIVATE -Wall
    PRIVATE -std=c99
    PRIVATE -MMD -MP
    PRIVATE -ffunction-sections -fdata-sections -fomit-frame-pointer
    )

# Include directories of libraries and externals
foreach(dep ${DEPENDENCIES})
    target_include_directories(${PROJECT_NAME}
        PRIVATE ${CMAKE_SOURCE_DIR}/${dep}/)
endforeach()

# Linker options for this project
target_link_options(${PROJECT_NAME}
    PUBLIC -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    PRIVATE -nostartfiles -static
    PRIVATE -Wl,--gc-sections,--entry,Reset_Handler
    PRIVATE -Wl,-Map=${PROJECT_NAME}.map
    PRIVATE -T ${LINKER_SCRIPT}
    )

target_link_libraries(${PROJECT_NAME}
    gcc
    c
    m
    )

# Print size of binary after linking
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} ${PROJECT_NAME}${OUTPUT_SUFFIX}
    )

# Install executable in [CMAKE_SOURCE_DIR]/bin
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
