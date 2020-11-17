cmake_minimum_required(VERSION 3.10)
project(unit-testing)

include(ExternalProject)

enable_testing()

get_filename_component(ROOT_PROJECT_SOURCE_DIR ${PROJECT_SOURCE_DIR} DIRECTORY)
get_filename_component(ROOT_PROJECT_SOURCE_DIR ${ROOT_PROJECT_SOURCE_DIR} DIRECTORY)


#option(UNIT_TESTING_MEM "Test memory management (malloc, free etc)" ON)
option(RUN_TESTS_DURING_BUILD "Run the tests wile building them" OFF)


#configure_file(
#    "${ROOT_PROJECT_SOURCE_DIR}/include/config_unit.h.in"
#    "${PROJECT_BINARY_DIR}/include/config_unit.h"
#    )

externalproject_add(
    project_cmocka
    PREFIX "${PROJECT_BINARY_DIR}/cmocka"
    INSTALL_COMMAND ""
	GIT_REPOSITORY "https://git.cryptomilk.org/projects/cmocka.git"
)

ExternalProject_Get_Property(project_cmocka source_dir)
ExternalProject_Get_Property(project_cmocka binary_dir)

include_directories("${source_dir}/include")
include_directories("${ROOT_PROJECT_SOURCE_DIR}/test/unit")
include_directories("${ROOT_PROJECT_SOURCE_DIR}/include")
include_directories("${ROOT_PROJECT_SOURCE_DIR}/bliss_allocator")
include_directories("${ROOT_PROJECT_SOURCE_DIR}/mpatch")
include_directories("${ROOT_PROJECT_SOURCE_DIR}/checkpoint")
include_directories("${ROOT_PROJECT_SOURCE_DIR}/checkpoint/arch/x86-linux")
include_directories("${PROJECT_BINARY_DIR}/include")
include_directories("${ROOT_PROJECT_SOURCE_DIR}/test/unit/util")

link_directories("${binary_dir}/src")
set(CMOCKA_LIBRARY cmocka)


SET( CMAKE_BUILD_TYPE Debug )

set(CMAKE_C_FLAGS
"${CMAKE_C_FLAGS} \
-O0 \
-g \
-Wall \
-Wno-unused \
"
)

find_program(LCOV "lcov")
IF(LCOV)
    message(STATUS "Using LCOV to generate coverage report")
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CodeCoverage.cmake)

    set(CMAKE_C_FLAGS
    "${CMAKE_C_FLAGS} \
    -coverage \
    "
    )
ELSE()
    message(STATUS "LCOV executable not found, no coverage report will be generated")
ENDIF()

# Add the unit tests
set(UNIT_TESTS
    test_cmocka
    test_bliss
    test_mpatch
    )

# Add the sources for the test, this is not done in the foreach loop because of
# dependencies. This would lead to a lot of exceptions.
add_executable(test_cmocka
    test_cmocka.c
    )

add_executable(test_bliss
    "${ROOT_PROJECT_SOURCE_DIR}/bliss_allocator/bliss_allocator_debug_util.c"
    "${ROOT_PROJECT_SOURCE_DIR}/bliss_allocator/bliss_allocator_nvm.c"
    "${ROOT_PROJECT_SOURCE_DIR}/bliss_allocator/bliss_allocator.c"
    bliss_allocator/test_bliss_allocator.c
    )
set_target_properties(test_bliss PROPERTIES COMPILE_FLAGS "-DBLISS_CUSTOM_MEMORY")

add_executable(test_mpatch
    "${ROOT_PROJECT_SOURCE_DIR}/bliss_allocator/bliss_allocator_nvm.c"
    "${ROOT_PROJECT_SOURCE_DIR}/bliss_allocator/bliss_allocator.c"
    "${ROOT_PROJECT_SOURCE_DIR}/mpatch/mpatch.c"
    "${ROOT_PROJECT_SOURCE_DIR}/mpatch/mpatch_nvm.c"
    util/asciitree.c
    mpatch/test_mpatch.c
    )
set_target_properties(test_mpatch PROPERTIES COMPILE_FLAGS "-DBLISS_CUSTOM_MEMORY")


# Add the test to cmake
# Add a custom command for executing the tests when building (depending on the option)
foreach(_UNIT_TEST ${UNIT_TESTS})
    target_link_libraries("${_UNIT_TEST}" ${CMOCKA_LIBRARY})
    add_dependencies("${_UNIT_TEST}" project_cmocka)

    if (${RUN_TESTS_DURING_BUILD})
        add_custom_command(TARGET "${_UNIT_TEST}" POST_BUILD COMMAND ${_UNIT_TEST})
    endif (${RUN_TESTS_DURING_BUILD})

    add_custom_target("${_UNIT_TEST}_run" "${_UNIT_TEST}" DEPENDS  ${_UNIT_TEST})
    add_test(NAME "${_UNIT_TEST}" COMMAND "${_UNIT_TEST}")

    list(APPEND TEST_BIN_LIST ${_UNIT_TEST})
endforeach()

message(STATUS "Tests: ${TEST_BIN_LIST}")
# https://stackoverflow.com/questions/733475/cmake-ctest-make-test-doesnt-build-tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND}
    DEPENDS ${TEST_BIN_LIST}
)

IF(LCOV)
    setup_target_for_coverage_lcov(
        NAME test_coverage
        EXECUTABLE ctest
        )
    set(COVERAGE_REPORT ${CMAKE_CURRENT_BINARY_DIR}/test_coverage/index.html)
    add_custom_target(test_coverage_show xdg-open ${COVERAGE_REPORT} DEPENDS ${COVERAGE_REPORT})
ENDIF()
