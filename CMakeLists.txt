cmake_minimum_required(VERSION 3.20)

project(OBVR VERSION 0.0.1 LANGUAGES CXX)

# OBVR laedt als Plugin in Oblivion.exe. Die ist 32 Bit, also muss auch die
# DLL 32 Bit sein - daran laesst sich nichts drehen.
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR
		"OBVR muss fuer x86 (32 Bit) gebaut werden, gefunden: ${CMAKE_SIZEOF_VOID_P}*8 Bit.\n"
		"MSVC:  cmake -A Win32 ...\n"
		"Linux: cmake --toolchain cmake/toolchain-linux-nosdk.cmake ...")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(OBVR_NO_WINSDK)
	# Muss hier stehen und nicht im Toolchain-File: CMakes Platform-Modul
	# setzt die Liste danach erneut und wuerde user32, gdi32, ole32 und
	# weitere anhaengen, die es ohne SDK nicht gibt.
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
	src/vr/HeadTracker.cpp
	src/vr/Quaternion.cpp
)

target_include_directories(OBVR PRIVATE src)

# Schaltet die Layout-Pruefungen in GameTypes.h scharf. Sie gelten nur fuer
# 32-Bit, wo die Zeigergroesse stimmt; nativ gebaute Tests lassen sie aus.
target_compile_definitions(OBVR PRIVATE OBVR_TARGET_32BIT)

set_target_properties(OBVR PROPERTIES
	OUTPUT_NAME "OBVR"
	PREFIX ""
	SUFFIX ".dll"
)

if(MSVC)
	target_compile_options(OBVR PRIVATE /W4 /permissive-)
	# Ohne statische Runtime muesste die passende Redistributable neben
	# Oblivion liegen; das will man einem Mod-Nutzer nicht zumuten.
	set_property(TARGET OBVR PROPERTY
		MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
	target_compile_options(OBVR PRIVATE -Wall -Wextra)
endif()

if(OBVR_NO_WINSDK)
	# Verifikationsbau unter Linux ohne Windows SDK. Siehe
	# cmake/toolchain-linux-nosdk.cmake.
	target_compile_definitions(OBVR PRIVATE OBVR_NO_WINSDK)
	# CMake wuerde sonst --dependent-lib=msvcrt anhaengen; die Bibliothek gibt
	# es hier nicht, und gelinkt wird ohnehin mit -nodefaultlib.
	set_property(TARGET OBVR PROPERTY MSVC_RUNTIME_LIBRARY "")
	add_subdirectory(cmake/imports)
	target_link_libraries(OBVR PRIVATE obvr_win32_imports)
endif()
