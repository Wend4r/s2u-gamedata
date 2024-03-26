if(NOT DYNLIBUTILS_DIR)
	message(FATAL_ERROR "DYNLIBUTILS_DIR is empty")
endif()

include_directories(
	${DYNLIBUTILS_DIR}
)

add_subdirectory("${DYNLIBUTILS_DIR}" memory_utils)
