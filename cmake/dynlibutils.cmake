if(NOT DYNLIBUTILS_DIR)
	message(FATAL_ERROR "DYNLIBUTILS_DIR is empty")
endif()

set(DYNLIBUTILS_INCLUDE_DIR
	${DYNLIBUTILS_DIR}/include
)

set(DYNLIBUTILS_BINARY_DIR memory_utils)

add_subdirectory("${DYNLIBUTILS_DIR}" ${DYNLIBUTILS_BINARY_DIR})
