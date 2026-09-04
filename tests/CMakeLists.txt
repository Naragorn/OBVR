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
	../src/camera/CastTrampoline.cpp
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

add_executable(culling_sync_test CullingSyncTest.cpp)

target_include_directories(culling_sync_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(culling_sync_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME culling_sync COMMAND culling_sync_test)

add_executable(around_call_test
	AroundCallTest.cpp
	../src/core/AroundCall.cpp
	../src/core/CodeWriter.cpp
)

target_include_directories(around_call_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(around_call_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME around_call COMMAND around_call_test)

add_executable(address_space_test AddressSpaceTest.cpp)

target_include_directories(address_space_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(address_space_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME address_space COMMAND address_space_test)

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

	add_executable(crosshair_cache_test
		CrosshairCacheTest.cpp
		../src/render/CrosshairCache.cpp
	)

	target_include_directories(crosshair_cache_test PRIVATE ../src)

	if(MSVC)
		target_compile_options(crosshair_cache_test PRIVATE /EHsc)
	else()
		target_compile_options(crosshair_cache_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME crosshair_cache COMMAND crosshair_cache_test)
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

# The name table is pure. ActiveMenuId, in the same file, reads Oblivion's
# memory through fixed addresses and stays untested - it links here and is
# never called, which is the same arrangement the other game-side files have.
add_executable(menu_type_test
	MenuTypeTest.cpp
	../src/game/MenuType.cpp
)

target_include_directories(menu_type_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(menu_type_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME menu_type COMMAND menu_type_test)

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

add_executable(layout_probe_test
	LayoutProbeTest.cpp
	../src/render/LayoutProbe.cpp
)

target_include_directories(layout_probe_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(layout_probe_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME layout_probe COMMAND layout_probe_test)

add_executable(game_camera_test
	GameCameraTest.cpp
	../src/game/GameCamera.cpp
)

target_include_directories(game_camera_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(game_camera_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME game_camera COMMAND game_camera_test)

# Only the pure walk over the setting list; resolving the collection in the
# game's memory needs the process and stays untested, like the probe's
# readback. The walk runs over lists this test builds itself, which is the
# point: every refusal flow can be exercised without an engine. Windows only
# because the walk logs its refusals, and the log writes through the plugin
# path - the same shape as config_test.
if(WIN32)
	add_executable(ini_settings_test
		IniSettingsTest.cpp
		../src/game/IniSettings.cpp
		../src/core/Log.cpp
		../src/core/Memory.cpp
		../src/platform/PluginPath.cpp
		../src/render/GameDevice.cpp
	)

	target_include_directories(ini_settings_test PRIVATE ../src)

	if(NOT MSVC)
		target_compile_options(ini_settings_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME ini_settings COMMAND ini_settings_test)

	# The decision half is pure; the file also carries the acting half, which
	# logs, so the same Windows-only linking as ini_settings applies.
	add_executable(ui_screen_size_test
		UiScreenSizeTest.cpp
		../src/render/UiScreenSize.cpp
		../src/core/Log.cpp
		../src/platform/PluginPath.cpp
		../src/render/GameDevice.cpp
	)

	target_include_directories(ui_screen_size_test PRIVATE ../src)

	if(NOT MSVC)
		target_compile_options(ui_screen_size_test PRIVATE -Wall -Wextra)
	endif()

	add_test(NAME ui_screen_size COMMAND ui_screen_size_test)
endif()

add_executable(menu_draw_test
	MenuDrawTest.cpp
	../src/ui/MenuFont.cpp
	../src/ui/MenuCanvas.cpp
)

target_include_directories(menu_draw_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(menu_draw_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME menu_draw COMMAND menu_draw_test)

add_executable(menu_model_test
	MenuModelTest.cpp
	../src/ui/MenuModel.cpp
)

target_include_directories(menu_model_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(menu_model_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME menu_model COMMAND menu_model_test)

add_executable(settings_list_test
	SettingsListTest.cpp
	../src/ui/SettingsList.cpp
	../src/ui/MenuModel.cpp
)

target_include_directories(settings_list_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(settings_list_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME settings_list COMMAND settings_list_test)

add_executable(menu_painter_test
	MenuPainterTest.cpp
	../src/ui/MenuPainter.cpp
	../src/ui/MenuCanvas.cpp
	../src/ui/MenuFont.cpp
	../src/ui/MenuModel.cpp
)

target_include_directories(menu_painter_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(menu_painter_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME menu_painter COMMAND menu_painter_test)

add_executable(settings_menu_test
	SettingsMenuTest.cpp
	../src/ui/SettingsMenu.cpp
	../src/ui/SettingsList.cpp
	../src/ui/MenuModel.cpp
)

target_include_directories(settings_menu_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(settings_menu_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME settings_menu COMMAND settings_menu_test)

add_executable(create_device_guard_test
	CreateDeviceGuardTest.cpp
	../src/render/CreateDeviceGuard.cpp
)

target_include_directories(create_device_guard_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(create_device_guard_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME create_device_guard COMMAND create_device_guard_test)

add_executable(branch_decode_test BranchDecodeTest.cpp)

target_include_directories(branch_decode_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(branch_decode_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME branch_decode COMMAND branch_decode_test)

add_executable(stall_detector_test StallDetectorTest.cpp)

target_include_directories(stall_detector_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(stall_detector_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME stall_detector COMMAND stall_detector_test)

add_executable(menu_pause_policy_test MenuPausePolicyTest.cpp)

target_include_directories(menu_pause_policy_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(menu_pause_policy_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME menu_pause_policy COMMAND menu_pause_policy_test)

add_executable(hand_input_test HandInputTest.cpp)

target_include_directories(hand_input_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(hand_input_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME hand_input COMMAND hand_input_test)

add_executable(node_name_list_test NodeNameListTest.cpp)
target_include_directories(node_name_list_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(node_name_list_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME node_name_list COMMAND node_name_list_test)

add_executable(hand_mode_test
	HandModeTest.cpp
	../src/vr/HandMode.cpp
	../src/vr/HeadOffset.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
	../src/core/Smoothing.cpp
)

target_include_directories(hand_mode_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(hand_mode_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME hand_mode COMMAND hand_mode_test)

add_executable(onboarding_test
	OnboardingTest.cpp
	../src/ui/Onboarding.cpp
	../src/ui/SettingsList.cpp
	../src/ui/MenuModel.cpp
)

target_include_directories(onboarding_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(onboarding_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME onboarding COMMAND onboarding_test)

add_executable(stack_scan_test StackScanTest.cpp)

target_include_directories(stack_scan_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(stack_scan_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME stack_scan COMMAND stack_scan_test)
