# Warnings
set(PLATFORM_COMPILER_OPTIONS
	-Wall -Wno-uninitialized -Wno-switch -Wno-unused
	-Wno-conversion-null -Wno-write-strings
	-Wno-invalid-offsetof -Wno-reorder
	-Wno-register

	# Others
	-mfpmath=sse -msse -fno-strict-aliasing
	-fno-threadsafe-statics -fvisibility=default
)

set(PLATFORM_LINKER_OPTIONS)

set(PLATFORM_COMPILE_DEFINITIONS
	-D_GLIBCXX_USE_CXX11_ABI=0
)
