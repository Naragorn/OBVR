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

add_executable(entry_detour_test
	EntryDetourTest.cpp
	../src/core/EntryDetour.cpp
	../src/core/CodeWriter.cpp
)

target_include_directories(entry_detour_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(entry_detour_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME entry_detour COMMAND entry_detour_test)

add_executable(quaternion_test
	QuaternionTest.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
	../src/core/Smoothing.cpp
)

target_include_directories(quaternion_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(quaternion_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME quaternion COMMAND quaternion_test)

add_executable(rotation_test
	RotationTest.cpp
	../src/core/Rotation.cpp
	../src/core/Smoothing.cpp
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

add_executable(head_offset_test
	HeadOffsetTest.cpp
	../src/vr/HeadOffset.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
	../src/core/Smoothing.cpp
)

target_include_directories(head_offset_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(head_offset_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME head_offset COMMAND head_offset_test)

add_executable(look_control_test
	LookControlTest.cpp
	../src/camera/LookControl.cpp
	../src/core/Rotation.cpp
	../src/core/Smoothing.cpp
)

target_include_directories(look_control_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(look_control_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME look_control COMMAND look_control_test)

add_executable(openvr_pose_test
	OpenVRPoseTest.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
	../src/core/Smoothing.cpp
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
		../src/render/GameDevice.cpp
		../src/platform/PluginPath.cpp
		../src/render/GameDevice.cpp
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
		../src/render/GameDevice.cpp
		../src/platform/PluginPath.cpp
		../src/render/GameDevice.cpp
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
		../src/vr/HeadOffset.cpp
		../src/vr/OpenVRBackend.cpp
		../src/vr/Quaternion.cpp
		../src/core/Rotation.cpp
		../src/core/Smoothing.cpp
		../src/core/Log.cpp
		../src/platform/PluginPath.cpp
		../src/render/GameDevice.cpp
		../src/platform/PluginPath.cpp
		../src/render/GameDevice.cpp
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
		../src/render/GameDevice.cpp
	)

	target_include_directories(plugin_path_test PRIVATE ../src)

	if(NOT MSVC)
		target_compile_options(plugin_path_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME plugin_path COMMAND plugin_path_test)
endif()

add_executable(test_pattern_test
	TestPatternTest.cpp
	../src/render/TestPattern.cpp
)

target_include_directories(test_pattern_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(test_pattern_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME test_pattern COMMAND test_pattern_test)

add_executable(submit_policy_test
	SubmitPolicyTest.cpp
	../src/render/SubmitPolicy.cpp
)

target_include_directories(submit_policy_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(submit_policy_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME submit_policy COMMAND submit_policy_test)

add_executable(eye_geometry_test
	EyeGeometryTest.cpp
	../src/render/EyeGeometry.cpp
)

target_include_directories(eye_geometry_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(eye_geometry_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME eye_geometry COMMAND eye_geometry_test)

add_executable(game_device_test
	GameDeviceTest.cpp
	../src/render/GameDevice.cpp
)

target_include_directories(game_device_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(game_device_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME game_device COMMAND game_device_test)

add_executable(dxvk_interop_test
	DxvkInteropTest.cpp
)

target_include_directories(dxvk_interop_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(dxvk_interop_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME dxvk_interop COMMAND dxvk_interop_test)

add_executable(lock_ledger_test
	LockLedgerTest.cpp
)

target_include_directories(lock_ledger_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(lock_ledger_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME lock_ledger COMMAND lock_ledger_test)

add_executable(pointer_set_test
	PointerSetTest.cpp
)

target_include_directories(pointer_set_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(pointer_set_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME pointer_set COMMAND pointer_set_test)

add_executable(palette_buckets_test
	PaletteBucketsTest.cpp
)

target_include_directories(palette_buckets_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(palette_buckets_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME palette_buckets COMMAND palette_buckets_test)

add_executable(bone_rebase_test
	BoneRebaseTest.cpp
)

target_include_directories(bone_rebase_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(bone_rebase_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME bone_rebase COMMAND bone_rebase_test)

add_executable(menu_shade_test
	MenuShadeTest.cpp
)

target_include_directories(menu_shade_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(menu_shade_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME menu_shade COMMAND menu_shade_test)

add_executable(d3d9_types_test
	D3D9TypesTest.cpp
)

target_include_directories(d3d9_types_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(d3d9_types_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME d3d9_types COMMAND d3d9_types_test)

add_executable(game_projection_test
	GameProjectionTest.cpp
	../src/render/GameProjection.cpp
)

target_include_directories(game_projection_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(game_projection_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME game_projection COMMAND game_projection_test)

if(WIN32)
	add_executable(present_hook_test
		PresentHookTest.cpp
		../src/render/PresentHook.cpp
		../src/core/Log.cpp
		../src/platform/PluginPath.cpp
		../src/render/GameDevice.cpp
	)

	target_include_directories(present_hook_test PRIVATE ../src)

	if(NOT MSVC)
		target_compile_options(present_hook_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME present_hook COMMAND present_hook_test)
endif()

add_executable(game_camera_test
	GameCameraTest.cpp
	../src/game/GameCamera.cpp
)

target_include_directories(game_camera_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(game_camera_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME game_camera COMMAND game_camera_test)
