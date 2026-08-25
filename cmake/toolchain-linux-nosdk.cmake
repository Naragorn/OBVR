# Verification build on Linux, without the Windows SDK and without MSVC.
#
# Purpose: confirm that OBVR compiles and links cleanly into a 32-bit Windows
# DLL, without needing a Windows machine or a multi-gigabyte SDK download.
# That is possible because OBVR needs nothing beyond kernel32 and msvcrt, and
# the import libraries are generated in cmake/imports.
#
# Usage:
#   cmake -B build --toolchain cmake/toolchain-linux-nosdk.cmake
#   cmake --build build
#
# The regular route for a release stays MSVC on Windows. This toolchain builds
# without the C++ standard library and without exceptions; it is deliberately
# a verification environment, not a comfortable one.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

find_program(OBVR_CLANG clang REQUIRED)
find_program(OBVR_LLD_LINK lld-link REQUIRED)
find_program(OBVR_DLLTOOL llvm-dlltool REQUIRED)

set(CMAKE_CXX_COMPILER "${OBVR_CLANG}")
set(CMAKE_CXX_COMPILER_TARGET i686-pc-windows-msvc)
set(CMAKE_LINKER "${OBVR_LLD_LINK}")

set(OBVR_NO_WINSDK ON CACHE BOOL "Build without the Windows SDK" FORCE)

# Without a CRT no complete test program can be linked, so CMake should only
# compile its compiler check rather than link it.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(OBVR_FREESTANDING_FLAGS
	"-ffreestanding -fno-exceptions -fno-rtti -nostdinc++ -fno-stack-protector")

set(CMAKE_CXX_FLAGS_INIT "${OBVR_FREESTANDING_FLAGS}")

# Otherwise CMake appends the usual Windows libraries (user32, gdi32,
# ole32 ...). Those do not exist without the SDK, and OBVR does not need them
# either - the required imports come from cmake/imports.
set(CMAKE_CXX_STANDARD_LIBRARIES "")

# Without a CRT there is no _DllMainCRTStartup, so the entry point has to
# point at DllMain explicitly. The @12 decoration is the stdcall signature.
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Xlinker /nodefaultlib -Xlinker /entry:DllMain@12")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
