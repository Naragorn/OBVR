cmake_minimum_required(VERSION 3.20)

project(OBVR VERSION 0.0.4 LANGUAGES CXX)

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
	src/camera/CastTrampoline.cpp
	src/camera/FrameLogic.cpp
	src/ui/MenuFont.cpp
	src/ui/MenuCanvas.cpp
	src/ui/MenuModel.cpp
	src/ui/MenuPainter.cpp
	src/ui/SettingsList.cpp
	src/ui/SettingsMenuLayer.cpp
	src/ui/SettingsMenu.cpp
	src/camera/LookControl.cpp
	src/core/CodeWriter.cpp
	src/core/Config.cpp
	src/core/AroundCall.cpp
	src/core/EntryDetour.cpp
	src/core/Log.cpp
	src/core/Memory.cpp
	src/core/Rotation.cpp
	src/core/Smoothing.cpp
	src/platform/Freestanding.cpp
	src/platform/GameWindow.cpp
	src/platform/ImportHook.cpp
	src/platform/PluginPath.cpp
	src/render/CursorPickHook.cpp
	src/render/CursorProbe.cpp
	src/render/EyeGeometry.cpp
	src/render/EyeMirror.cpp
	src/render/UiScreenSize.cpp
	src/game/AimAtSource.cpp
	src/game/DialogZoom.cpp
	src/game/GameCamera.cpp
	src/game/MenuMode.cpp
	src/game/MenuBackground.cpp
	src/game/CrosshairTarget.cpp
	src/game/FirstPersonArms.cpp
	src/game/MenuType.cpp
	src/game/PlayerAim.cpp
	src/render/GameDevice.cpp
	src/render/GameProjection.cpp
	src/render/GameFrame.cpp
	src/render/EyeTextures.cpp
	src/render/HeadsetRenderer.cpp
	src/render/PresentHook.cpp
	src/render/HudLayer.cpp
	src/render/CrosshairLayer.cpp
	src/render/InterfaceRenderHook.cpp
	src/render/LayoutProbe.cpp
	src/render/ResolutionHook.cpp
	src/render/CreateDeviceGuard.cpp
	src/render/SceneRenderHook.cpp
	src/render/SceneGraphProbe.cpp
	src/render/InteropBracket.cpp
	src/render/SubmitPolicy.cpp
	src/render/TestPattern.cpp
	src/vr/HeadOffset.cpp
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
