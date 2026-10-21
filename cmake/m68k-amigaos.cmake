# CMake toolchain for the Amiga build (PLAN.md D2): bebbo's GCC 16.2 for
# m68k-amigaos with libnix. build.sh passes it with -DCMAKE_TOOLCHAIN_FILE and
# TC16 in the environment (default ~/amiga-gcc162-install).
set(CMAKE_SYSTEM_NAME AmigaOS)
set(CMAKE_SYSTEM_PROCESSOR m68k)
# CMake has no AmigaOS platform of its own: ours (cmake/Platform/AmigaOS.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

if(DEFINED ENV{TC16})
	set(TC16 "$ENV{TC16}")
else()
	set(TC16 "$ENV{HOME}/amiga-gcc162-install")
endif()
set(CMAKE_C_COMPILER   "${TC16}/bin/m68k-amigaos-gcc")
set(CMAKE_CXX_COMPILER "${TC16}/bin/m68k-amigaos-g++")
set(CMAKE_AR           "${TC16}/bin/m68k-amigaos-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB       "${TC16}/bin/m68k-amigaos-ranlib" CACHE FILEPATH "")

# D2: 68020 code with the 68881 FPU (fintrz for float-to-int), libnix.
# -fno-strict-aliasing comes from d3es's own flags.
# C++17: GCC 16 defaults to C++20, where `requires` is a keyword, and Doom 3
# uses it as a name (game/Game_local.h). AMIGA_INCLUDES (from build.sh): the
# GL ES headers (ogles2's copy of Khronos') and the like.
set(CMAKE_C_FLAGS_INIT   "-mcpu=68020 -m68881 -noixemul $ENV{AMIGA_INCLUDES}")
# -fuse-cxa-atexit: static destructors in reverse construction order through
# the port's __cxa_atexit (src/amiga/cxa_atexit.c), not libnix's DTOR list.
set(CMAKE_CXX_FLAGS_INIT "-std=gnu++17 -fuse-cxa-atexit -mcpu=68020 -m68881 -noixemul $ENV{AMIGA_INCLUDES}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-mcpu=68020 -m68881 -noixemul")

# CMake's test programs are only compiled, not linked or run
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH "${TC16}/m68k-amigaos")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
