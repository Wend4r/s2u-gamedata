# GameData
# Copyright (C) 2023-2024 Wend4r
# Licensed under the GPLv3 license. See LICENSE file in the project root for details.

if(NOT DYNLIBUTILS_DIR)
	message(FATAL_ERROR "DYNLIBUTILS_DIR is empty")
endif()

set(DYNLIBUTILS_INCLUDE_DIRS
	${DYNLIBUTILS_INCLUDE_DIRS}

	${DYNLIBUTILS_DIR}/include
)
