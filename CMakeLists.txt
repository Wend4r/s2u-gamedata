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

if(NOT DYNLIBUTILS_DIR)
	message(WARNING "DYNLIBUTILS_DIR is empty")
	set(DYNLIBUTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../memory_utils)
endif()

if(NOT SOURCESDK_DIR)
	message(WARNING "SOURCESDK_DIR is empty")
	set(SOURCESDK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../hl2sdk-wend4r/hl2sdk-cs2)
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

target_compile_options(${PROJECT_NAME} PRIVATE ${COMPILER_OPTIONS})
target_link_options(${PROJECT_NAME} PRIVATE ${LINKER_OPTIONS})

target_compile_definitions(${PROJECT_NAME} PRIVATE ${COMPILE_DEFINITIONS} ${SOURCESDK_DEFINTIONS})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIR} ${DYNLIBUTILS_INCLUDE_DIR} ${SOURCESDK_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE ${DYNLIBUTILS_BINARY_DIR} ${SOURCESDK_LIBRARIES})
