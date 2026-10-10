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
	../src/game/WorldPickTrampoline.cpp
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
	../src/core/Rotation.cpp
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
		WaterVRReplayStub.cpp
		../src/vr/OpenVRBackend.cpp
		../src/perf/Profiler.cpp
		../src/perf/ProfileLogic.cpp
		../src/perf/GpuQueries.cpp
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
		../src/perf/ProfileLogic.cpp
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
		WaterVRReplayStub.cpp
		../src/vr/HeadTracker.cpp
		../src/vr/HeadOffset.cpp
		../src/vr/OpenVRBackend.cpp
		../src/perf/Profiler.cpp
		../src/perf/ProfileLogic.cpp
		../src/perf/GpuQueries.cpp
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
		../src/core/Log.cpp
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
	../src/core/Log.cpp
	../src/platform/PluginPath.cpp
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
	../src/core/Log.cpp
	../src/platform/PluginPath.cpp
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
		../src/perf/Profiler.cpp
		../src/perf/ProfileLogic.cpp
		../src/perf/GpuQueries.cpp
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

add_executable(reload_gate_test ReloadGateTest.cpp)

target_include_directories(reload_gate_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(reload_gate_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME reload_gate COMMAND reload_gate_test)

add_executable(bracket_owner_test BracketOwnerTest.cpp)

target_include_directories(bracket_owner_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(bracket_owner_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME bracket_owner COMMAND bracket_owner_test)

add_executable(menu_pause_policy_test MenuPausePolicyTest.cpp)

target_include_directories(menu_pause_policy_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(menu_pause_policy_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME menu_pause_policy COMMAND menu_pause_policy_test)

add_executable(input_route_test InputRouteTest.cpp)

target_include_directories(input_route_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(input_route_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME input_route COMMAND input_route_test)

add_executable(hand_input_test HandInputTest.cpp)

target_include_directories(hand_input_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(hand_input_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME hand_input COMMAND hand_input_test)

add_executable(teleport_test TeleportTest.cpp)

target_include_directories(teleport_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(teleport_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME teleport COMMAND teleport_test)

add_executable(controller_actions_test ControllerActionsTest.cpp)
target_include_directories(controller_actions_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(controller_actions_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME controller_actions COMMAND controller_actions_test)

add_executable(bone_pin_test
	BonePinTest.cpp
	../src/core/Rotation.cpp
	../src/core/Smoothing.cpp
)
target_include_directories(bone_pin_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(bone_pin_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME bone_pin COMMAND bone_pin_test)

add_executable(laser_geometry_test
	LaserGeometryTest.cpp
	../src/core/Rotation.cpp
)
target_include_directories(laser_geometry_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(laser_geometry_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME laser_geometry COMMAND laser_geometry_test)

add_executable(snap_turn_test
	SnapTurnTest.cpp
	../src/core/Rotation.cpp
)
target_include_directories(snap_turn_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(snap_turn_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME snap_turn COMMAND snap_turn_test)

add_executable(body_placement_test
	BodyPlacementTest.cpp
	../src/core/Rotation.cpp
)
target_include_directories(body_placement_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(body_placement_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME body_placement COMMAND body_placement_test)

add_executable(melee_hit_test
	MeleeHitTest.cpp
	../src/core/Rotation.cpp
	../src/core/Smoothing.cpp
)
target_include_directories(melee_hit_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(melee_hit_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME melee_hit COMMAND melee_hit_test)

add_executable(backface_pass_test BackfacePassTest.cpp)
target_include_directories(backface_pass_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(backface_pass_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME backface_pass COMMAND backface_pass_test)

add_executable(world_push_test WorldPushTest.cpp)
target_include_directories(world_push_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(world_push_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME world_push COMMAND world_push_test)

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

add_executable(native_settings_test
	NativeSettingsTest.cpp
	../src/ui/NativeSettings.cpp
	../src/ui/SettingsList.cpp
	../src/ui/MenuModel.cpp
)
target_include_directories(native_settings_test PRIVATE ../src)
add_test(NAME native_settings COMMAND native_settings_test)

add_executable(native_onboarding_test NativeOnboardingTest.cpp)
target_include_directories(native_onboarding_test PRIVATE ../src)
add_test(NAME native_onboarding COMMAND native_onboarding_test)

add_executable(crosshair_target_test CrosshairTargetTest.cpp)
target_include_directories(crosshair_target_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(crosshair_target_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME crosshair_target COMMAND crosshair_target_test)

add_executable(stack_scan_test StackScanTest.cpp)

target_include_directories(stack_scan_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(stack_scan_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME stack_scan COMMAND stack_scan_test)

add_executable(leaf_batch_test LeafBatchTest.cpp ../src/core/AroundCall.cpp ../src/core/CodeWriter.cpp)
target_include_directories(leaf_batch_test PRIVATE ../src)
add_test(NAME leaf_batch COMMAND leaf_batch_test)

add_executable(water_reflection_test WaterReflectionTest.cpp)
target_include_directories(water_reflection_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(water_reflection_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME water_reflection COMMAND water_reflection_test)

add_executable(water_reprojection_test
	WaterReprojectionTest.cpp
	../src/core/Rotation.cpp
)
target_include_directories(water_reprojection_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(water_reprojection_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME water_reprojection COMMAND water_reprojection_test)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
	add_test(NAME native_menu_assets
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/NativeMenuAssetsTest.py)
	add_test(NAME water_shader_read
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/WaterShaderReadTest.py)
	add_test(NAME water_position_control
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/WaterPositionControlTest.py)
	add_test(NAME water_camera_decode
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/WaterCameraDecodeTest.py)
	add_test(NAME water_camera_read
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/WaterCameraReadTest.py)
	add_test(NAME water_visual_harness
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/WaterVisualHarnessTest.py)
	add_test(NAME recenter_load_harness
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/RecenterLoadHarnessTest.py)
endif()

add_executable(water_vr_test_plan_test WaterVRTestPlanTest.cpp)
target_include_directories(water_vr_test_plan_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(water_vr_test_plan_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME water_vr_test_plan COMMAND water_vr_test_plan_test)

add_executable(profile_logic_test
	ProfileLogicTest.cpp
	../src/perf/ProfileLogic.cpp
)
target_include_directories(profile_logic_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(profile_logic_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME profile_logic COMMAND profile_logic_test)

add_executable(gpu_queries_test
	GpuQueriesTest.cpp
	../src/perf/GpuQueries.cpp
)
target_include_directories(gpu_queries_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(gpu_queries_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME gpu_queries COMMAND gpu_queries_test)

if(Python3_Interpreter_FOUND)
	add_test(NAME analyze_performance
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/AnalyzePerformanceTest.py)
	add_test(NAME controller_action_assets
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ControllerActionAssetsTest.py)
endif()

add_executable(scroll_menu_test ScrollMenuTest.cpp)
target_include_directories(scroll_menu_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(scroll_menu_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME scroll_menu COMMAND scroll_menu_test)

add_executable(scroll_session_test ScrollSessionTest.cpp)
target_include_directories(scroll_session_test PRIVATE ../src)
add_test(NAME scroll_session COMMAND scroll_session_test)

add_executable(vr_menu_input_test VRMenuInputTest.cpp)
target_include_directories(vr_menu_input_test PRIVATE ../src)
add_test(NAME vr_menu_input COMMAND vr_menu_input_test)

add_executable(vr_menu_bridge_test VRMenuBridgeTest.cpp)
	target_sources(vr_menu_bridge_test PRIVATE
		../src/game/VRMenuBridge.cpp
		../src/game/VRMenuIntentStub.cpp
		../src/core/EntryDetour.cpp
		../src/core/AroundCall.cpp
		../src/core/CodeWriter.cpp
		../src/core/Memory.cpp
		../src/core/Log.cpp
		../src/platform/PluginPath.cpp
	)
target_include_directories(vr_menu_bridge_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(vr_menu_bridge_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME vr_menu_bridge COMMAND vr_menu_bridge_test)

add_executable(update_notice_test
	UpdateNoticeTest.cpp
)
target_include_directories(update_notice_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(update_notice_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME update_notice COMMAND update_notice_test)

add_executable(controller_projection_test
	ControllerProjectionTest.cpp
)
target_include_directories(controller_projection_test PRIVATE ../src)
if(NOT MSVC)
	target_compile_options(controller_projection_test PRIVATE -Wall -Wextra)
endif()
add_test(NAME controller_projection COMMAND controller_projection_test)

add_executable(control_bindings_test ControlBindingsTest.cpp)

target_include_directories(control_bindings_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(control_bindings_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME control_bindings COMMAND control_bindings_test)

add_executable(hand_script_test
	HandScriptTest.cpp
	../src/vr/Quaternion.cpp
)

target_include_directories(hand_script_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(hand_script_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME hand_script COMMAND hand_script_test)

add_executable(quick_menu_test QuickMenuTest.cpp)

target_include_directories(quick_menu_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(quick_menu_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME quick_menu COMMAND quick_menu_test)

add_executable(quick_menu_painter_test
	QuickMenuPainterTest.cpp
	../src/ui/QuickMenuPainter.cpp
	../src/ui/StowSpotPainter.cpp
	../src/ui/MenuCanvas.cpp
	../src/ui/MenuFont.cpp
)

target_include_directories(quick_menu_painter_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(quick_menu_painter_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME quick_menu_painter COMMAND quick_menu_painter_test)

add_executable(holster_test
	HolsterTest.cpp
	../src/vr/Quaternion.cpp
)

target_include_directories(holster_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(holster_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME holster COMMAND holster_test)

add_executable(fist_test FistTest.cpp ../src/vr/Quaternion.cpp)

target_include_directories(fist_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(fist_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME fist COMMAND fist_test)

add_executable(holster_fit_test HolsterFitTest.cpp)

target_include_directories(holster_fit_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(holster_fit_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME holster_fit COMMAND holster_fit_test)

add_executable(stow_test StowTest.cpp ../src/vr/Quaternion.cpp)

target_include_directories(stow_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(stow_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME stow COMMAND stow_test)

add_executable(icon_test
	IconTest.cpp
	../src/render/DdsImage.cpp
	../src/ui/MenuCanvas.cpp
	../src/ui/MenuFont.cpp
)

target_include_directories(icon_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(icon_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME icon COMMAND icon_test)

add_executable(quick_key_pages_test QuickKeyPagesTest.cpp)

target_include_directories(quick_key_pages_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(quick_key_pages_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME quick_key_pages COMMAND quick_key_pages_test)

add_executable(arm_stump_test ArmStumpTest.cpp)

target_include_directories(arm_stump_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(arm_stump_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME arm_stump COMMAND arm_stump_test)

add_executable(finger_test FingerTest.cpp)

target_include_directories(finger_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(finger_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME finger COMMAND finger_test)

add_executable(weapon_draw_speed_test WeaponDrawSpeedTest.cpp)

target_include_directories(weapon_draw_speed_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(weapon_draw_speed_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME weapon_draw_speed COMMAND weapon_draw_speed_test)

add_executable(thumb_pose_test ThumbPoseTest.cpp)

target_include_directories(thumb_pose_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(thumb_pose_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME thumb_pose COMMAND thumb_pose_test)

add_executable(walk_direction_test WalkDirectionTest.cpp)

target_include_directories(walk_direction_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(walk_direction_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME walk_direction COMMAND walk_direction_test)

add_executable(teleport_noise_test TeleportNoiseTest.cpp)

target_include_directories(teleport_noise_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(teleport_noise_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME teleport_noise COMMAND teleport_noise_test)

add_executable(teleport_sound_test TeleportSoundTest.cpp)

target_include_directories(teleport_sound_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(teleport_sound_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME teleport_sound COMMAND teleport_sound_test)

add_executable(hand_body_test HandBodyTest.cpp)

target_include_directories(hand_body_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(hand_body_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME hand_body COMMAND hand_body_test)

add_executable(player_capsule_test PlayerCapsuleTest.cpp)

target_include_directories(player_capsule_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(player_capsule_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME player_capsule COMMAND player_capsule_test)

add_executable(shove_test ShoveTest.cpp)

target_include_directories(shove_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(shove_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME shove COMMAND shove_test)

add_executable(hand_hud_test HandHudTest.cpp)

target_include_directories(hand_hud_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(hand_hud_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME hand_hud COMMAND hand_hud_test)

add_executable(dialog_panel_test DialogPanelTest.cpp)

target_include_directories(dialog_panel_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(dialog_panel_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME dialog_panel COMMAND dialog_panel_test)

add_executable(lead_test LeadTest.cpp)

target_include_directories(lead_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(lead_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME lead COMMAND lead_test)

add_executable(throw_test ThrowTest.cpp)

target_include_directories(throw_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(throw_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME throw COMMAND throw_test)

add_executable(two_hand_test TwoHandTest.cpp)

target_include_directories(two_hand_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(two_hand_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME two_hand COMMAND two_hand_test)

add_executable(archery_test ArcheryTest.cpp)

target_include_directories(archery_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(archery_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME archery COMMAND archery_test)

add_executable(flat_follow_test FlatFollowTest.cpp)

target_include_directories(flat_follow_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(flat_follow_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME flat_follow COMMAND flat_follow_test)

add_executable(sound_slice_test SoundSliceTest.cpp)

target_include_directories(sound_slice_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(sound_slice_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME sound_slice COMMAND sound_slice_test)

add_executable(compass_heading_test CompassHeadingTest.cpp)

target_include_directories(compass_heading_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(compass_heading_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME compass_heading COMMAND compass_heading_test)

add_executable(equip_while_acting_test EquipWhileActingTest.cpp)

target_include_directories(equip_while_acting_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(equip_while_acting_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME equip_while_acting COMMAND equip_while_acting_test)

add_executable(hud_depth_test HudDepthTest.cpp)

target_include_directories(hud_depth_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(hud_depth_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME hud_depth COMMAND hud_depth_test)

add_executable(target_marker_test TargetMarkerTest.cpp)

target_include_directories(target_marker_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(target_marker_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME target_marker COMMAND target_marker_test)

add_executable(scene_frame_test SceneFrameTest.cpp)

target_include_directories(scene_frame_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(scene_frame_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME scene_frame COMMAND scene_frame_test)

add_executable(pick_hold_test PickHoldTest.cpp)

target_include_directories(pick_hold_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(pick_hold_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME pick_hold COMMAND pick_hold_test)

# Engine-facing dialogue tests mock patching; calling conventions require x86 MSVC.
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 4)
	add_executable(dialog_camera_hook_test DialogCameraHookTest.cpp)
	target_include_directories(dialog_camera_hook_test PRIVATE ../src)
	add_test(NAME dialog_camera_hook COMMAND dialog_camera_hook_test)
endif()

add_executable(insult_test InsultTest.cpp)

target_include_directories(insult_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(insult_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME insult COMMAND insult_test)

add_executable(yield_test YieldTest.cpp)

target_include_directories(yield_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(yield_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME yield COMMAND yield_test)

add_executable(vr_keyboard_test VrKeyboardTest.cpp)

target_include_directories(vr_keyboard_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(vr_keyboard_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME vr_keyboard COMMAND vr_keyboard_test)

add_executable(menu_haptics_test MenuHapticsTest.cpp)

target_include_directories(menu_haptics_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(menu_haptics_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME menu_haptics COMMAND menu_haptics_test)

add_executable(weapon_weight_test
	WeaponWeightTest.cpp
	../src/vr/Quaternion.cpp
	../src/core/Rotation.cpp
)

target_include_directories(weapon_weight_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(weapon_weight_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME weapon_weight COMMAND weapon_weight_test)

add_executable(reach_open_test ReachOpenTest.cpp)

target_include_directories(reach_open_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(reach_open_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME reach_open COMMAND reach_open_test)

add_executable(blade_contact_test BladeContactTest.cpp)

target_include_directories(blade_contact_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(blade_contact_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME blade_contact COMMAND blade_contact_test)

add_executable(parry_test ParryTest.cpp)

target_include_directories(parry_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(parry_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME parry COMMAND parry_test)

add_executable(combat_reach_test CombatReachTest.cpp)

target_include_directories(combat_reach_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(combat_reach_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME combat_reach COMMAND combat_reach_test)

add_executable(slow_approach_test SlowApproachTest.cpp)

target_include_directories(slow_approach_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(slow_approach_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME slow_approach COMMAND slow_approach_test)

add_executable(grapple_test GrappleTest.cpp)

target_include_directories(grapple_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(grapple_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME grapple COMMAND grapple_test)

add_executable(ref_fingerprint_test RefFingerprintTest.cpp)

target_include_directories(ref_fingerprint_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(ref_fingerprint_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME ref_fingerprint COMMAND ref_fingerprint_test)

add_executable(menu_cursor_test MenuCursorTest.cpp)

target_include_directories(menu_cursor_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(menu_cursor_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME menu_cursor COMMAND menu_cursor_test)

add_executable(combat_ring_test CombatRingTest.cpp)

target_include_directories(combat_ring_test PRIVATE ../src)

if(NOT MSVC)
	target_compile_options(combat_ring_test PRIVATE -Wall -Wextra)
endif()

add_test(NAME combat_ring COMMAND combat_ring_test)
