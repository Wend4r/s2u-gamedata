cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

if(POLICY CMP0092)
	cmake_policy(SET CMP0092 NEW) # Don't add -W3 warning level by default.
endif()

project(gamedata
	VERSION 1.0.0
	DESCRIPTION "GameData"
	HOMEPAGE_URL "https://github.com/Wend4r/s2u-gamedata"
	LANGUAGES C CXX
)

if(DYNLIBUTILS_DIR)
	file(TO_CMAKE_PATH "${DYNLIBUTILS_DIR}" DYNLIBUTILS_DIR)
else()
	set(DYNLIBUTILS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/memory_utils")
endif()

if(SOURCESDK_DIR)
	file(TO_CMAKE_PATH "${SOURCESDK_DIR}" SOURCESDK_DIR)
else()
	set(SOURCESDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/hl2sdk")
endif()

set(INCLUDE_DIR "include")
set(SOURCE_DIR "src")

include("cmake/platform/shared.cmake")

if(LINUX)
	include("cmake/platform/linux.cmake")
elseif(WINDOWS)
	include("cmake/platform/windows.cmake")
endif()

include("cmake/dynlibutils.cmake")
include("cmake/sourcesdk.cmake")

set(SOURCE_FILES
	${SOURCE_DIR}/gamedata.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_EXTENSIONS OFF)

target_compile_options(${PROJECT_NAME} PRIVATE ${PLATFORM_COMPILER_OPTIONS})
target_link_options(${PROJECT_NAME} PRIVATE ${PLATFORM_LINKER_OPTIONS})

target_compile_definitions(${PROJECT_NAME} PRIVATE ${PLATFORM_COMPILE_DEFINITIONS} ${SOURCESDK_COMPILE_DEFINTIONS})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIR} ${DYNLIBUTILS_INCLUDE_DIR} ${SOURCESDK_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE ${DYNLIBUTILS_BINARY_DIR} ${SOURCESDK_LIBRARIES})
