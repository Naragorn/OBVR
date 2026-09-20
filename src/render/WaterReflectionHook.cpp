#include "render/WaterReflectionHook.h"
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
#include "render/InterfaceRenderHook.h"
#include "render/WaterReflection.h"
#include "test/WaterVRTestRuntime.h"

namespace obvr::render {
namespace {

const UInt8 kEntry[addr::kWaterRenderReflectionsEntryLength] = {
	0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x6A,0xFF,
};
using RenderFn = void(__fastcall*)(void*, void*, NiAVObject*, void*);
RenderFn g_original = nullptr;
bool g_reuseReported = false;
bool g_captureReported = false;
bool g_managerStateKnown = false;
UInt32 g_lastManagerResource0 = 0;
UInt32 g_lastManagerResource1 = 0;
bool g_reflectionsOptionKnown = false;
bool g_lastReflectionsOption = false;
void* g_keptManagerResource = nullptr;

using NiPointerAssignFn = void(__thiscall*)(void**, void*);

void RecoverWaterManagerResource(void* self) {
	if (self == nullptr) return;
	auto* slot = reinterpret_cast<void**>(static_cast<UInt8*>(self) + 4);
	const bool enabled = *reinterpret_cast<const UInt8*>(addr::kUseWaterReflections) != 0;
	if (!g_reflectionsOptionKnown || enabled != g_lastReflectionsOption) {
		OBVR_LOG("Water manager lifecycle: bUseWaterReflections=%u", enabled);
		g_reflectionsOptionKnown = true;
		g_lastReflectionsOption = enabled;
	}
	const UInt32 current = reinterpret_cast<UInt32>(*slot);
	const UInt32 kept = reinterpret_cast<UInt32>(g_keptManagerResource);
	const WaterResourceAction action =
		ChooseWaterResourceAction(enabled, current, kept);
	auto assign = reinterpret_cast<NiPointerAssignFn>(addr::kNiPointerAssign);
	if (action == WaterResourceAction::Remember) {
		assign(&g_keptManagerResource, *slot);
		OBVR_LOG("Water manager lifecycle: retained reflection resource %08X", current);
	} else if (action == WaterResourceAction::Restore) {
		assign(slot, g_keptManagerResource);
		OBVR_LOG("Water manager lifecycle: restored reflection resource %08X after Off -> On",
		         kept);
	}
}

class RestoreCameraFrustum {
public:
	RestoreCameraFrustum(NiAVObject* camera, float horizontal, float vertical)
		: frustum_(reinterpret_cast<game::NiFrustum*>(
			reinterpret_cast<UInt8*>(camera) + game::kNiCameraFrustumOffset)),
		  original_(*frustum_) {
		active_ = ScaleWaterCaptureFrustum(*frustum_, horizontal, vertical);
		if (!active_) frustum_ = nullptr;
	}
	~RestoreCameraFrustum() {
		if (frustum_ != nullptr) *frustum_ = original_;
	}
	bool Active() const { return active_; }
private:
	game::NiFrustum* frustum_;
	game::NiFrustum original_;
	bool active_ = false;
};

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
	RecoverWaterManagerResource(self);
	const UInt32 managerResource0 = self != nullptr
		? *reinterpret_cast<const UInt32*>(self) : 0;
	const UInt32 managerResource1 = self != nullptr
		? *reinterpret_cast<const UInt32*>(static_cast<const UInt8*>(self) + 4) : 0;
	if (!g_managerStateKnown || managerResource0 != g_lastManagerResource0 ||
	    managerResource1 != g_lastManagerResource1) {
		OBVR_LOG("Water manager resources: self=%08X first=%08X second=%08X",
		         reinterpret_cast<UInt32>(self), managerResource0, managerResource1);
		g_managerStateKnown = true;
		g_lastManagerResource0 = managerResource0;
		g_lastManagerResource1 = managerResource1;
	}
	const Config& config = GetConfig();
	NiTransform playerLocal{}, stableWorld{}, captureLocal{};
	const bool cyclopean = UsesCyclopeanWaterCapture(config.waterReflectionMode);
	const bool playerTransformsValid = cyclopean && camera != nullptr &&
		camera::GetHeadIndependentWaterCameraTransforms(playerLocal, stableWorld);
	NiTransform captureParent{};
	captureParent.rot = NiMatrix33::Identity();
	captureParent.scale = 1.0f;
	if (camera != nullptr && camera->parent != nullptr)
		captureParent = camera->parent->worldTransform;
	const NiTransform renderWorld = WaterRenderCameraFromBody(stableWorld);
	const bool transformsValid = playerTransformsValid &&
		BuildWaterCameraLocalFromParent(captureParent, renderWorld, captureLocal);
	const bool stable = CanUseStableWaterCapture(
		config.stableWaterReflections, config.waterReflectionMode,
		WaterReprojectionReady(), camera != nullptr, transformsValid);
	if (!stable) {
		SetWaterCaptureProjectionScale(1.0f, 1.0f);
		SetWaterReflectionCapture(NiTransform{}, false);
		g_original(self, edx, camera, shadowScene);
		return;
	}
	const WaterStereoPass stereoPass = GetWaterStereoPass();
	if (test::SuppressWaterVRFirstEyeReflection(stereoPass == WaterStereoPass::First))
		return;
	if (!ShouldRenderWaterReflection(true, stereoPass,
	                                 WaterReflectionRenderedThisStereoPair())) {
		NoteWaterReflectionReused();
		if (!g_reuseReported) {
			g_reuseReported = true;
			OBVR_LOG("Water reflection: second eye reuses the first-eye original target");
		}
		return;
	}

	// Render exactly once into Oblivion's own reflection target. Only the
	// camera used by that nested scene is changed; its target, pixel shader,
	// samplers and wave path stay owned by the game. The main water vertex
	// shader receives this same camera's projective matrix through c13-c16.
	constexpr float kHorizontalCaptureScale = 3.0f;
	constexpr float kVerticalCaptureScale = 1.5f;
	const game::NiFrustum inputFrustum = *reinterpret_cast<const game::NiFrustum*>(
		reinterpret_cast<const UInt8*>(camera) + game::kNiCameraFrustumOffset);
	const NiTransform inputWorld = camera->worldTransform;
	RestoreCameraFrustum frustum(camera, kHorizontalCaptureScale, kVerticalCaptureScale);
	const float horizontalScale = frustum.Active() ? kHorizontalCaptureScale : 1.0f;
	const float verticalScale = frustum.Active() ? kVerticalCaptureScale : 1.0f;
	SetWaterCaptureProjectionScale(horizontalScale, verticalScale);
	SetWaterReflectionCapture(stableWorld, true);
	SetWaterReflectionSubpass(true);
	const bool probeTarget = test::WaterVRReplayActive();
	if (probeTarget) BeginWaterReflectionTargetProbe();
	{
		// The water renderer owns a separate camera node. Convert the desired
		// head-independent world pose through that node's actual parent rather
		// than assigning the player camera's unrelated local transform.
		RestoreCameraTransforms restore(camera, captureLocal);
		g_original(self, edx, camera, shadowScene);
	}
	SetWaterReflectionSubpass(false);
	if (probeTarget) EndWaterReflectionTargetProbe();
	NoteWaterReflectionRendered();
	test::ObserveWaterVRReflectionRendered();
	if (!g_captureReported) {
		g_captureReported = true;
		NiTransform liveWorld{};
		const bool liveValid = camera::GetCurrentCameraWorldTransform(liveWorld);
		for (unsigned row = 0; row < 3; ++row) {
			OBVR_LOG("Water reflection axes: row=%u native=(%.9g,%.9g,%.9g) requested=(%.9g,%.9g,%.9g) bodyValid=%u body=(%.9g,%.9g,%.9g)",
				row, inputWorld.rot.data[row][0], inputWorld.rot.data[row][1], inputWorld.rot.data[row][2],
				stableWorld.rot.data[row][0], stableWorld.rot.data[row][1], stableWorld.rot.data[row][2],
				liveValid, liveWorld.rot.data[row][0], liveWorld.rot.data[row][1], liveWorld.rot.data[row][2]);
		}
		// Entry camera evidence only: the native routine may create another
		// camera internally. Do not treat this as its measured render matrix.
		OBVR_LOG("Water reflection entry: frustum l=%.9g r=%.9g t=%.9g b=%.9g n=%.9g f=%.9g ortho=%u scale=%.9g,%.9g",
			inputFrustum.l, inputFrustum.r, inputFrustum.t, inputFrustum.b,
			inputFrustum.n, inputFrustum.f, unsigned(inputFrustum.o),
			horizontalScale, verticalScale);
		OBVR_LOG("Water reflection entry: original position=%.9g,%.9g,%.9g requested=%.9g,%.9g,%.9g",
			inputWorld.pos.x, inputWorld.pos.y, inputWorld.pos.z,
			stableWorld.pos.x, stableWorld.pos.y, stableWorld.pos.z);
		OBVR_LOG("Water reflection: head-independent camera rendered into the original target");
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
	OBVR_LOG("Water reflection: original-target camera/projection hook installed");
	return true;
}

}  // namespace obvr::render
