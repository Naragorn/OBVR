#include "render/WaterReflectionHook.h"
#include <cmath>

#include "camera/CameraHook.h"
#include "core/Config.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Rotation.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/GameCamera.h"
#include "render/WaterReprojection.h"
#include "render/WaterReflectionBlend.h"
#include "render/GameDevice.h"
#include "render/InterfaceRenderHook.h"

namespace obvr::render {
namespace {

const UInt8 kEntry[addr::kWaterRenderReflectionsEntryLength] = {
	0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x6A,0xFF,
};
using RenderFn = void(__fastcall*)(void*, void*, NiAVObject*, void*);
RenderFn g_original = nullptr;
bool g_reuseReported = false;
constexpr float kCaptureYawDegrees = 60.0f;

class RestoreCameraTransforms {
public:
	explicit RestoreCameraTransforms(NiAVObject* camera, const NiTransform& local)
		: camera_(camera), originalLocal_(camera->localTransform) {
		camera_->localTransform = local;
		// The renderer reads the camera's derived world transform and frustum.
		// Updating both raw fields leaves that derived state describing the old eye.
		game::UpdateNodeTransforms(camera_);
	}
	~RestoreCameraTransforms() {
		camera_->localTransform = originalLocal_;
		game::UpdateNodeTransforms(camera_);
	}
private:
	NiAVObject* camera_;
	NiTransform originalLocal_;
};

void __fastcall Hooked(void* self, void* edx, NiAVObject* camera, void* shadowScene) {
	const Config& config = GetConfig();
	NiTransform stableLocal{}, stableWorld{};
	const bool cyclopean = UsesCyclopeanWaterCapture(config.waterReflectionMode);
	const bool transformsValid = cyclopean && camera != nullptr &&
		camera::GetHeadIndependentWaterCameraTransforms(stableLocal, stableWorld);
	const bool stable = CanUseStableWaterCapture(
		config.stableWaterReflections, config.waterReflectionMode,
		WaterReprojectionReady(), camera != nullptr, transformsValid);
	if (!stable) {
		SetWaterCaptureProjectionScale(1.0f, 1.0f);
		SetWaterReflectionCapture(NiTransform{}, false);
		g_original(self, edx, camera, shadowScene);
		return;
	}
	if (!ShouldRenderWaterReflection(true, GetWaterStereoPass())) {
		NoteWaterReflectionReused();
		if (!g_reuseReported) {
			g_reuseReported = true;
			OBVR_LOG("Water reflection: second eye reuses three first-eye captures");
		}
		return;
	}

	SetWaterCaptureProjectionScale(1.0f, 1.0f);
	const float yaw[kWaterReflectionCaptureCount] = {
		-kCaptureYawDegrees, 0.0f, kCaptureYawDegrees
	};
	bool stored = true;
	// Oblivion binds its 256x256 reflection target for the first capture and
	// then keeps that same target bound for the two following passes. Keep our
	// reference for the complete fan-out; restarting the probe per slot loses
	// the source before slots one and two can copy it.
	//
	// This is a nested engine scene, not one of the main world draws. The
	// second stereo eye deliberately skips it, so its water shader calls must
	// not consume entries from the first-eye main-pass replay queue.
	SetWaterReflectionSubpass(true);
	BeginWaterReflectionTargetProbe();
	for (unsigned slot = 0; slot < kWaterReflectionCaptureCount; ++slot) {
		NiTransform local{}, world{};
		BuildWaterReflectionCaptureTransform(stableWorld, yaw[slot], world);
		NiTransform parent{};
		parent.rot = NiMatrix33::Identity();
		parent.scale = 1.0f;
		if (camera->parent != nullptr) parent = camera->parent->worldTransform;
		if (!BuildWaterCameraLocalFromParent(parent, world, local)) {
			stored = false;
			OBVR_LOG("Water reflection: capture slot %u could not derive camera-local transform", slot);
			break;
		}
		SetWaterReflectionCaptureSlot(slot, world, true);
		RestoreCameraTransforms restore(camera, local);
		g_original(self, edx, camera, shadowScene);
		stored = StoreWaterReflectionTarget(GetGameDevice(), slot) && stored;
	}
	EndWaterReflectionTargetProbe();
	SetWaterReflectionSubpass(false);
	NoteWaterReflectionRendered();
	if (!stored) {
		SetWaterReflectionCapture(NiTransform{}, false);
		OBVR_LOG("Water reflection: one of three normal-FOV captures could not be stored");
	}
}

}  // namespace

bool InstallWaterReflectionHook() {
	if (g_original != nullptr) {
		SetWaterReflectionHookReady(true);
		return true;
	}
	SetWaterReflectionHookReady(false);
	if (!mem::Verify(addr::kWaterRenderReflections, kEntry, sizeof(kEntry))) {
		OBVR_LOG("Water reflection: capture entry differs; reprojected mode falls back to vanilla");
		return false;
	}
	UInt8* trampoline = static_cast<UInt8*>(mem::AllocExecutable(16));
	if (trampoline == nullptr) return false;
	const UInt32 trampolineSize = mem::BuildEntryTrampoline(
		trampoline, 16, reinterpret_cast<UInt32>(trampoline),
		addr::kWaterRenderReflections, kEntry, sizeof(kEntry));
	UInt8 patch[sizeof(kEntry)]{};
	const UInt32 patchSize = mem::BuildEntryPatch(
		patch, sizeof(patch), addr::kWaterRenderReflections,
		reinterpret_cast<UInt32>(&Hooked), sizeof(kEntry));
	if (trampolineSize == 0 || patchSize != sizeof(patch)) return false;
	g_original = reinterpret_cast<RenderFn>(trampoline);
	if (!mem::SafeWrite(addr::kWaterRenderReflections, patch, sizeof(patch))) {
		g_original = nullptr;
		return false;
	}
	SetWaterReflectionHookReady(true);
	OBVR_LOG("Water reflection: capture/projection coupling hook installed");
	return true;
}

}  // namespace obvr::render
