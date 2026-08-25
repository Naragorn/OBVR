cmake_minimum_required(VERSION 3.20)

# A standalone project, deliberately not part of the DLL build.
#
# OBVR.dll is compiled for 32-bit Windows, while the tests run natively on the
# development machine. Doing both in one CMake run would mean driving two
# toolchains at once. The building blocks under test are pure byte generation
# and do not depend on the target architecture.
#
#   cmake -B build-tests tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure

project(OBVRTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(trampoline_test
	TrampolineTest.cpp
	../src/camera/CameraTrampoline.cpp
	../src/core/CodeWriter.cpp
)

target_include_directories(trampoline_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(trampoline_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME trampoline COMMAND trampoline_test)

add_executable(quaternion_test
	QuaternionTest.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
)

target_include_directories(quaternion_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(quaternion_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME quaternion COMMAND quaternion_test)

add_executable(rotation_test
	RotationTest.cpp
	../src/core/Rotation.cpp
)

target_include_directories(rotation_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(rotation_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME rotation COMMAND rotation_test)

add_executable(frame_logic_test
	FrameLogicTest.cpp
	../src/camera/FrameLogic.cpp
)

target_include_directories(frame_logic_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(frame_logic_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME frame_logic COMMAND frame_logic_test)

add_executable(openvr_pose_test
	OpenVRPoseTest.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
)

target_include_directories(openvr_pose_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(openvr_pose_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME openvr_pose COMMAND openvr_pose_test)

# The backend test calls LoadLibrary and therefore only builds on Windows,
# unlike the other tests which are plain arithmetic. Keeping it behind WIN32
# preserves the native test run on Linux, where the rest still applies.
if(WIN32)
	add_executable(openvr_backend_test
		OpenVRBackendTest.cpp
		../src/vr/OpenVRBackend.cpp
		../src/vr/Quaternion.cpp
		../src/core/Log.cpp
		../src/platform/PluginPath.cpp
	)

	target_include_directories(openvr_backend_test PRIVATE ../src)

	if(NOT MSVC)
		target_compile_options(openvr_backend_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME openvr_backend COMMAND openvr_backend_test)

	# Config reads through GetPrivateProfileString, so this one is Windows only
	# as well.
	add_executable(config_test
		ConfigTest.cpp
		../src/core/Config.cpp
		../src/core/Log.cpp
		../src/platform/PluginPath.cpp
	)

	target_include_directories(config_test PRIVATE ../src)

	if(NOT MSVC)
		target_compile_options(config_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME config COMMAND config_test)

	# HeadTracker owns an OpenVRBackend, which calls LoadLibrary, so this one
	# is Windows only too. The OpenVR source is exercised in its unavailable
	# state, which is the state that matters for users without SteamVR.
	add_executable(head_tracker_test
		HeadTrackerTest.cpp
		../src/vr/HeadTracker.cpp
		../src/vr/OpenVRBackend.cpp
		../src/vr/Quaternion.cpp
		../src/core/Rotation.cpp
		../src/core/Log.cpp
		../src/platform/PluginPath.cpp
	)

	target_include_directories(head_tracker_test PRIVATE ../src)

	if(NOT MSVC)
		target_compile_options(head_tracker_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME head_tracker COMMAND head_tracker_test)

	# Both anchors are built on GetModuleFileNameA, so Windows only.
	add_executable(plugin_path_test
		PluginPathTest.cpp
		../src/platform/PluginPath.cpp
	)

	target_include_directories(plugin_path_test PRIVATE ../src)

	if(NOT MSVC)
		target_compile_options(plugin_path_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME plugin_path COMMAND plugin_path_test)
endif()
