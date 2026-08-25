cmake_minimum_required(VERSION 3.20)

project(OBVR VERSION 0.0.3 LANGUAGES CXX)

# OBVR loads as a plugin into Oblivion.exe. That is 32 bit, so the DLL has to
# be 32 bit as well - there is no way around it.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR
		"OBVR must be built for x86 (32 bit), found: ${CMAKE_SIZEOF_VOID_P}*8 bit.\n"
		"MSVC:  cmake -A Win32 ...\n"
		"Linux: cmake --toolchain cmake/toolchain-linux-nosdk.cmake ...")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(OBVR_NO_WINSDK)
	# This has to live here rather than in the toolchain file: CMake's
	# platform module sets the list again afterwards and would append user32,
	# gdi32, ole32 and others that do not exist without the SDK.
	set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
endif()

add_library(OBVR SHARED
	src/Plugin.cpp
	src/camera/CameraHook.cpp
	src/camera/CameraTrampoline.cpp
	src/core/CodeWriter.cpp
	src/core/Config.cpp
	src/core/Log.cpp
	src/core/Memory.cpp
	src/core/Rotation.cpp
	src/platform/Freestanding.cpp
	src/platform/PluginPath.cpp
	src/vr/HeadTracker.cpp
	src/vr/OpenVRBackend.cpp
	src/vr/Quaternion.cpp
)

target_include_directories(OBVR PRIVATE src)

# Arms the layout checks in GameTypes.h. They only hold for 32 bit, where the
# pointer size matches; natively built tests leave them out.
target_compile_definitions(OBVR PRIVATE OBVR_TARGET_32BIT)

set_target_properties(OBVR PROPERTIES
	OUTPUT_NAME "OBVR"
	PREFIX ""
	SUFFIX ".dll"
)

if(MSVC)
	target_compile_options(OBVR PRIVATE /W4 /permissive-)
	# Without a static runtime the matching redistributable would have to sit
	# next to Oblivion; that is not something to ask of a mod user.
	set_property(TARGET OBVR PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
	target_compile_options(OBVR PRIVATE -Wall -Wextra)
endif()

if(OBVR_NO_WINSDK)
	# Verification build on Linux without the Windows SDK. See
	# cmake/toolchain-linux-nosdk.cmake.
	target_compile_definitions(OBVR PRIVATE OBVR_NO_WINSDK)
	# Otherwise CMake would append --dependent-lib=msvcrt; that library does
	# not exist here, and linking happens with -nodefaultlib anyway.
	set_property(TARGET OBVR PROPERTY MSVC_RUNTIME_LIBRARY "")
	add_subdirectory(cmake/imports)
	target_link_libraries(OBVR PRIVATE obvr_win32_imports)
endif()
