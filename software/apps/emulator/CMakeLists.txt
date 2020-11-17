# Author: TU Delft Sustainable Systems Laboratory
# License: MIT License

project(emulator VERSION 1.0.0 LANGUAGES C ASM)

set(CHECKPOINT_DEPENDENCIES
    libs/mpatch/checkpoint
    libs/mpatch/checkpoint/arch/arm
    libs/mpatch/checkpoint/arch/arm/reg
    libs/mpatch/checkpoint/arch/arm/reg
    libs/mpatch/checkpoint/code-id
    libs/mpatch/checkpoint/bss
    libs/mpatch/checkpoint/data
    libs/mpatch/checkpoint/stack
    libs/mpatch/bliss_allocator
    libs/mpatch/mpatch
    )

set(CHECKPOINT_DEPENDENCIES_EXCLUDE
    libs/mpatch/bliss_allocator/bliss_allocator_debug_util.c
    )

set(CODE_ID_SRC "libs/mpatch/checkpoint/code-id/code_id.c")

set(DEPENDENCIES
    external/CMSIS/ARM/Include
    external/CMSIS/AmbiqMicro/Include
    external/utils
    external/mcu/apollo3/
    external/mcu/apollo3/hal/
    config/
    platforms/
    libs/emulator/
    external/F746_Gameboy/inc
    libs/display
    libs/buttons
    libs/fram/
    libs/memtracker/
    libs/jit/
    libs/cartridge/
    ${CHECKPOINT_DEPENDENCIES}
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

# Exclude marked source files
foreach (excl ${DEPENDENCIES_EXCLUDE})
    #message("Excluding ${CMAKE_SOURCE_DIR}/${excl}")
    list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/${excl}")
endforeach()

# Add executable target
add_executable(${PROJECT_NAME} ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-Dprintf=am_util_stdio_printf")

# Always regenerate the code ID used to (in)validate checkpoint restores
add_custom_target(rebuild-code-id-emulator ALL
                  COMMAND ${CMAKE_COMMAND} -E touch
                  ${CMAKE_SOURCE_DIR}/${CODE_ID_SRC})
add_dependencies(${PROJECT_NAME} rebuild-code-id)

# Change target suffix to have <name>.out
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ${OUTPUT_SUFFIX})

# Defines (-D)
target_compile_definitions(${PROJECT_NAME}
    PRIVATE HWREV=1
    PRIVATE AM_UTIL_FAULTISR_PRINT
    PRIVATE AM_HAL_DISABLE_API_VALIDATION
    PRIVATE CHECKPOINT
)

# Compiler options for this project
target_compile_options(${PROJECT_NAME}
    #PUBLIC -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    PUBLIC -mfloat-abi=soft
    PRIVATE -Ofast # -O0 for development or -Ofast
    PRIVATE -g -gdwarf-3
    PRIVATE -Wall
    PRIVATE -std=c99
    PRIVATE -MMD -MP
    PRIVATE -ffunction-sections -fdata-sections -fomit-frame-pointer -fno-common
    )

# Include directories of libraries and externals
foreach(dep ${DEPENDENCIES})
    target_include_directories(${PROJECT_NAME}
        PRIVATE ${CMAKE_SOURCE_DIR}/${dep}/)
endforeach()

# Add the config directory in the project source dir
target_include_directories(${PROJECT_NAME} PRIVATE "${PROJECT_SOURCE_DIR}/config")

# Linker options for this project
target_link_options(${PROJECT_NAME}
    #PUBLIC -mfpu=fpv4-sp-d16 -mfloat-abi=hard
    PUBLIC -mfloat-abi=soft
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
