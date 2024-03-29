if(NOT SOURCESDK_DIR)
	message(FATAL_ERROR "SOURCESDK_DIR is empty")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8) # 64-bit
	add_definitions(
		-DPLATFORM_64BITS -DX64BITS
	)
else()
	set(SIZEOF_BITS ${CMAKE_SIZEOF_VOID_P})
	math(EXPR SIZEOF_BITS "${SIZEOF_BITS}*8")
	message(FATAL_ERROR "${SIZEOF_BITS}-bit platform is not supported")
endif()

set(SOURCESDK_DEFINTIONS)

if(LINUX)
	set(SOURCESDK_DEFINTIONS
		${SOURCESDK_DEFINTIONS}

		-D_LINUX -DPOSIX -DLINUX -DCOMPILER_GCC
		-Dstricmp=strcasecmp -D_stricmp=strcasecmp -D_strnicmp=strncasecmp
		-Dstrnicmp=strncasecmp -D_snprintf=snprintf
		-D_vsnprintf=vsnprintf -D_alloca=alloca -Dstrcmpi=strcasecmp
	)
endif()

if(MSVC)
	set(SOURCESDK_DEFINTIONS
		${SOURCESDK_DEFINTIONS}

		-DCOMPILER_MSVC -DCOMPILER_MSVC64
	)
endif()

set(SOURCESDK_INCLUDE_DIR
	${SOURCESDK_DIR}/common
	${SOURCESDK_DIR}/game/shared
	${SOURCESDK_DIR}/game/server
	${SOURCESDK_DIR}/public/engine
	${SOURCESDK_DIR}/public/entity2
	${SOURCESDK_DIR}/public/game/server
	${SOURCESDK_DIR}/public/mathlib
	${SOURCESDK_DIR}/public/tier0
	${SOURCESDK_DIR}/public/tier1
	${SOURCESDK_DIR}/public
	${SOURCESDK_DIR}/thirdparty/protobuf-3.21.8/src
	${SOURCESDK_DIR}
)

set(SOURCESDK_LIB_DIR ${SOURCESDK_DIR}/lib)

if(WINDOWS)
	set(SOURCESDK_LIB_PLATFORM_DIR "${SOURCESDK_LIB_DIR}/public/win64")

	set(SOURCESDK_LIBRARIES
		${SOURCESDK_LIB_PLATFORM_DIR}/tier0.lib
		# ${SOURCESDK_LIB_PLATFORM_DIR}/tier1.lib
		# ${SOURCESDK_LIB_PLATFORM_DIR}/interfaces.lib
		# ${SOURCESDK_LIB_PLATFORM_DIR}/mathlib.lib
	)
elseif(LINUX)
	set(SOURCESDK_LIB_PLATFORM_DIR "${SOURCESDK_LIB_DIR}/linux64")

	set(SOURCESDK_LIBRARIES
		${SOURCESDK_LIB_PLATFORM_DIR}/libtier0.so
		# ${SOURCESDK_LIB_PLATFORM_DIR}/tier1.a
		# ${SOURCESDK_LIB_PLATFORM_DIR}/interfaces.a
		# ${SOURCESDK_LIB_PLATFORM_DIR}/mathlib.a
	)
else()
	message(FATAL_ERROR "Unsupported platform")
endif()
