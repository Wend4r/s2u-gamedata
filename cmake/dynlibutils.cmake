# GameData
# Copyright (C) 2023-2024 Wend4r
# Licensed under the GPLv3 license. See LICENSE file in the project root for details.

if(NOT DYNLIBUTILS_DIR)
	message(FATAL_ERROR "DYNLIBUTILS_DIR is empty")
endif()

set(DYNLIBUTILS_BINARY_DIR "cpp-memory_utils")

set(DYNLIBUTILS_INCLUDE_DIR
	${DYNLIBUTILS_INCLUDE_DIR}

	${DYNLIBUTILS_DIR}/include
)

if(LINUX)
	set(DYNLIBUTILS_COMPILE_DEFINITIONS
		${DYNLIBUTILS_COMPILE_DEFINITIONS}

		-D_GLIBCXX_USE_CXX11_ABI=0
	)
endif()

add_subdirectory(${DYNLIBUTILS_DIR} ${DYNLIBUTILS_BINARY_DIR})
target_compile_definitions(${DYNLIBUTILS_BINARY_DIR} PRIVATE ${DYNLIBUTILS_COMPILE_DEFINITIONS})
