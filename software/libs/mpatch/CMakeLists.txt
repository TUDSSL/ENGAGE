cmake_minimum_required(VERSION 3.10)

include(ExternalProject)

project(MPatch C ASM)

# The default C Flags
set(CMAKE_C_FLAGS
"${CMAKE_C_FLAGS} \
-O1 \
-Wall \
-fno-strict-aliasing \
"
)


SET(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp")


add_subdirectory(test/unit)

add_custom_target(
	tests-run
	"${PROJECT_SOURCE_DIR}/test/unit/run-tests.sh" "${PROJECT_BINARY_DIR}/test/unit/"
	#DEPENDS tests
	)


#add_subdirectory(src)
