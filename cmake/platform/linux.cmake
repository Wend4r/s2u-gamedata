# Warnings
set(COMPILER_OPTIONS
	-Wall -Wno-uninitialized -Wno-switch -Wno-unused
	-Wno-conversion-null -Wno-write-strings
	-Wno-invalid-offsetof -Wno-reorder
	-Wno-register

	# Others
	-mfpmath=sse -msse -fno-strict-aliasing
	-fno-threadsafe-statics -fvisibility=default
)

set(LINKER_OPTIONS)

include(CheckCCompilerFlag)

check_c_compiler_flag("-fPIC" COMPILER_SUPPORTS_FPIC)

if(COMPILER_SUPPORTS_FPIC)
	set(COMPILER_OPTIONS
		${COMPILER_OPTIONS}
		-fPIC
	)
endif()
