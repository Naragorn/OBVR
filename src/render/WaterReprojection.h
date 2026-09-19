#pragma once

#include <cmath>

#include "core/Types.h"
#include "game/NiMath.h"
#include "game/GameCamera.h"

namespace obvr::render {

constexpr unsigned kWaterReflectionCaptureCount = 3;

// Measured at the native reflection entry: render-camera columns are body
// columns Y,Z,X. The shader reprojection still uses the body-camera basis.
inline NiTransform WaterRenderCameraFromBody(const NiTransform& body) {
	NiTransform camera = body;
	for (unsigned row = 0; row < 3; ++row) {
		camera.rot.data[row][0] = body.rot.data[row][1];
		camera.rot.data[row][1] = body.rot.data[row][2];
		camera.rot.data[row][2] = body.rot.data[row][0];
	}
	return camera;
}

enum class WaterReflectionMode : UInt32 {
	Vanilla = 0,
	StaticColor = 1,
	CyclopeanCapture = 2,
	Reprojected = 3,
};

enum class WaterStereoPass : UInt32 {
	Single = 0,
	First = 1,
	Second = 2,
};

enum class WaterShaderLifecycleAction : UInt32 {
	WaitForShaders = 0,
	KeepReady = 1,
	KeepRefused = 2,
	Rebuild = 3,
};

// Water settings can destroy and recreate Oblivion's shader wrappers while
// the D3D device itself stays alive. Decide from the live wrapper state on
// every shader bind; device identity alone is not a sufficient cache key.
inline WaterShaderLifecycleAction DecideWaterShaderLifecycle(
	bool deviceChanged, bool currentValid, bool identityMatches,
	bool ready, bool refused) {
	if (!currentValid) return WaterShaderLifecycleAction::WaitForShaders;
	if (deviceChanged || !identityMatches) return WaterShaderLifecycleAction::Rebuild;
	if (ready) return WaterShaderLifecycleAction::KeepReady;
	if (refused) return WaterShaderLifecycleAction::KeepRefused;
	return WaterShaderLifecycleAction::Rebuild;
}

inline bool UsesCyclopeanWaterCapture(WaterReflectionMode mode) {
	return mode == WaterReflectionMode::CyclopeanCapture ||
	       mode == WaterReflectionMode::Reprojected;
}

inline bool UsesWaterReprojectionShader(WaterReflectionMode mode) {
	return mode == WaterReflectionMode::CyclopeanCapture ||
	       mode == WaterReflectionMode::Reprojected;
}

inline bool ShouldSelectStableWaterVertexShader(WaterReflectionMode mode,
                                                bool inReflectionSubpass,
                                                bool shadersReady,
                                                bool captureStable) {
	return !inReflectionSubpass && UsesWaterReprojectionShader(mode) &&
	       shadersReady && captureStable;
}

inline bool ShouldRenderWaterReflection(bool stable, WaterStereoPass pass,
                                        bool renderedThisStereoPair) {
	// The second eye may reuse only content actually produced by the first eye
	// of this pair. A live OFF -> ON settings transition can suppress the
	// first-eye callback entirely; treating an older target as current then
	// leaves the re-enabled eye sampling stale or uninitialised contents.
	return !stable || pass != WaterStereoPass::Second || !renderedThisStereoPair;
}

enum class WaterProjectionPlan : UInt32 {
	ReprojectCurrent = 0,
	CaptureAndStore = 1,
	ReuseStored = 2,
	MissingStored = 3,
};

inline WaterProjectionPlan ChooseWaterProjectionPlan(WaterReflectionMode mode,
                                                     WaterStereoPass pass,
                                                     bool storedValid) {
	if (mode != WaterReflectionMode::CyclopeanCapture)
		return WaterProjectionPlan::ReprojectCurrent;
	if (pass != WaterStereoPass::Second)
		return WaterProjectionPlan::CaptureAndStore;
	return storedValid ? WaterProjectionPlan::ReuseStored
	                   : WaterProjectionPlan::MissingStored;
}

inline bool CanUseStableWaterCapture(bool enabled, WaterReflectionMode mode,
                                     bool reprojectionReady, bool cameraValid,
                                     bool transformsValid) {
	return enabled && UsesCyclopeanWaterCapture(mode) &&
	       (!UsesWaterReprojectionShader(mode) || reprojectionReady) &&
	       cameraValid && transformsValid;
}

// HMD pose changes must not restart an otherwise unchanged reflection capture.
// The hook compares the head-independent world transform before calling this.
inline bool CanReuseWaterCapture(bool cacheValid, bool texturesReady,
                                 bool transformMatches) {
	return cacheValid && texturesReady && transformMatches;
}

struct WaterMatrix {
	float m[4][4]{};
};

struct WaterReprojectionSnapshot {
	WaterMatrix current{};
	WaterMatrix world{};
	WaterMatrix reprojected{};
	NiTransform live{};
	NiTransform capture{};
	UInt32 serial = 0;
	UInt32 captureProjectionSerial = 0;
	UInt32 reflectionRenderSerial = 0;
	UInt32 reflectionReuseSerial = 0;
	WaterStereoPass pass = WaterStereoPass::Single;
	float captureHorizontalScale = 1.0f;
	float captureVerticalScale = 1.0f;
	bool reusedCaptureProjection = false;
	UInt32 storedWaterDrawCount = 0;
	UInt32 replayedWaterDrawCount = 0;
	bool fallback = false;
};

inline WaterMatrix MultiplyWaterMatrix(const WaterMatrix& a, const WaterMatrix& b) {
	WaterMatrix result{};
	for (unsigned row = 0; row < 4; ++row)
		for (unsigned column = 0; column < 4; ++column)
			for (unsigned k = 0; k < 4; ++k)
				result.m[row][column] += a.m[row][k] * b.m[k][column];
	return result;
}

inline bool InvertWaterMatrix(const WaterMatrix& input, WaterMatrix& output) {
	float augmented[4][8]{};
	for (unsigned row = 0; row < 4; ++row) {
		for (unsigned column = 0; column < 4; ++column)
			augmented[row][column] = input.m[row][column];
		augmented[row][row + 4] = 1.0f;
	}
	for (unsigned column = 0; column < 4; ++column) {
		unsigned pivot = column;
		float magnitude = std::fabs(augmented[pivot][column]);
		for (unsigned row = column + 1; row < 4; ++row) {
			const float candidate = std::fabs(augmented[row][column]);
			if (candidate > magnitude) {
				pivot = row;
				magnitude = candidate;
			}
		}
		if (!(magnitude > 1.0e-8f) || !std::isfinite(magnitude)) return false;
		if (pivot != column)
			for (unsigned i = 0; i < 8; ++i) {
				const float value = augmented[column][i];
				augmented[column][i] = augmented[pivot][i];
				augmented[pivot][i] = value;
			}
		const float divisor = augmented[column][column];
		for (unsigned i = 0; i < 8; ++i) augmented[column][i] /= divisor;
		for (unsigned row = 0; row < 4; ++row) {
			if (row == column) continue;
			const float factor = augmented[row][column];
			for (unsigned i = 0; i < 8; ++i)
				augmented[row][i] -= factor * augmented[column][i];
		}
	}
	for (unsigned row = 0; row < 4; ++row)
		for (unsigned column = 0; column < 4; ++column) {
			const float value = augmented[row][column + 4];
			if (!std::isfinite(value)) return false;
			output.m[row][column] = value;
		}
	return true;
}

inline void BuildHeadIndependentWaterCamera(
 const NiTransform& cyclopeanLocal, const NiTransform& cyclopeanWorld,
 const NiTransform& parentWorld, const NiMatrix33& baseLocalRotation,
 const NiPoint3& baseLocalPosition,
 NiTransform& localOut, NiTransform& worldOut) {
 // The cyclopean transform was read after OBVR applied the HMD offset and
 // first-eye step. Keep the engine's pre-HMD local position instead, so the
 // capture camera does not move when the wearer leans or shifts the headset.
 localOut=cyclopeanLocal; localOut.pos=baseLocalPosition; localOut.rot=baseLocalRotation;
 worldOut=cyclopeanWorld; worldOut.rot=parentWorld.rot*baseLocalRotation;
 worldOut.pos=parentWorld.pos + parentWorld.rot*(baseLocalPosition*parentWorld.scale);
 worldOut.scale=parentWorld.scale*localOut.scale;
}

inline bool ScaleWaterProjectionRows(WaterMatrix& matrix,float horizontal,float vertical) {
 if(!(horizontal>=1.0f && horizontal<=16.0f && vertical>=1.0f && vertical<=16.0f) ||
    !std::isfinite(horizontal) || !std::isfinite(vertical)) return false;
 for(unsigned column=0;column<4;++column) {
  matrix.m[0][column]/=horizontal;
  matrix.m[1][column]/=vertical;
 }
 return true;
}

inline bool ScaleWaterCaptureFrustum(game::NiFrustum& frustum,
                                     float horizontal, float vertical) {
	if (!(horizontal >= 1.0f && horizontal <= 16.0f &&
	      vertical >= 1.0f && vertical <= 16.0f) ||
	    !std::isfinite(horizontal) || !std::isfinite(vertical) ||
	    !std::isfinite(frustum.l) || !std::isfinite(frustum.r) ||
	    !std::isfinite(frustum.t) || !std::isfinite(frustum.b) ||
	    !std::isfinite(frustum.n) || !std::isfinite(frustum.f) ||
	    !(frustum.n > 0.0f) || !(frustum.f > frustum.n) ||
	    frustum.o || !(frustum.l < 0.0f) || !(frustum.r > 0.0f) ||
	    !(frustum.b < 0.0f) || !(frustum.t > 0.0f)) {
		return false;
	}
	frustum.l *= horizontal;
	frustum.r *= horizontal;
	frustum.t *= vertical;
	frustum.b *= vertical;
	return std::isfinite(frustum.l) && std::isfinite(frustum.r) &&
	       std::isfinite(frustum.t) && std::isfinite(frustum.b);
}

inline WaterMatrix CameraWorldMatrix(const NiTransform& camera) {
	WaterMatrix result{};
	for (unsigned row = 0; row < 3; ++row)
		for (unsigned column = 0; column < 3; ++column)
			result.m[row][column] = camera.rot.data[row][column] * camera.scale;
	result.m[0][3] = camera.pos.x;
	result.m[1][3] = camera.pos.y;
	result.m[2][3] = camera.pos.z;
	result.m[3][3] = 1.0f;
	return result;
}

// Convert a desired camera world transform to the local transform under a
// specific scene-graph parent. Reflection cameras are separate nodes from the
// player camera; using the player local transform directly leaves the parent
// to reintroduce HMD motion.
inline NiMatrix33 InverseWaterRotation(const NiMatrix33& rotation) {
 NiMatrix33 inverse{};
 for (unsigned row = 0; row < 3; ++row)
  for (unsigned column = 0; column < 3; ++column)
   inverse.data[row][column] = rotation.data[column][row];
 return inverse;
}

inline bool BuildWaterCameraLocalFromParent(const NiTransform& parent,
                                             const NiTransform& desiredWorld,
                                             NiTransform& localOut) {
 if (!(std::isfinite(parent.scale) && std::fabs(parent.scale) > 1.0e-5f) ||
     !std::isfinite(desiredWorld.scale)) return false;
 const NiMatrix33 inverseParent = InverseWaterRotation(parent.rot);
 localOut.rot = inverseParent * desiredWorld.rot;
 localOut.pos = (inverseParent * (desiredWorld.pos - parent.pos)) * (1.0f / parent.scale);
 localOut.scale = desiredWorld.scale / parent.scale;
 for (unsigned row = 0; row < 3; ++row)
  for (unsigned column = 0; column < 3; ++column)
   if (!std::isfinite(localOut.rot.data[row][column])) return false;
 return std::isfinite(localOut.pos.x) && std::isfinite(localOut.pos.y) &&
        std::isfinite(localOut.pos.z) && std::isfinite(localOut.scale);
}

// Oblivion's water c4-c7 WorldMat has the live camera position subtracted.
// Convert it back to absolute world space before combining it with absolute
// NiCamera transforms. Otherwise the recovered projection contains a second,
// yaw-dependent camera translation. See water-input-projection runtime logs.
inline bool RestoreWaterWorldOrigin(const WaterMatrix& relativeWorld,
                                   const NiPoint3& cameraPosition,
                                   WaterMatrix& absoluteWorld) {
	if (!std::isfinite(cameraPosition.x) || !std::isfinite(cameraPosition.y) ||
	    !std::isfinite(cameraPosition.z)) return false;
	for (unsigned row = 0; row < 4; ++row)
		for (unsigned column = 0; column < 4; ++column)
			if (!std::isfinite(relativeWorld.m[row][column])) return false;
	if (relativeWorld.m[3][0] != 0.0f || relativeWorld.m[3][1] != 0.0f ||
	    relativeWorld.m[3][2] != 0.0f || relativeWorld.m[3][3] != 1.0f) return false;
	absoluteWorld = relativeWorld;
	absoluteWorld.m[0][3] += cameraPosition.x;
	absoluteWorld.m[1][3] += cameraPosition.y;
	absoluteWorld.m[2][3] += cameraPosition.z;
	return std::isfinite(absoluteWorld.m[0][3]) &&
	       std::isfinite(absoluteWorld.m[1][3]) &&
	       std::isfinite(absoluteWorld.m[2][3]);
}

// Mlive = P * inverse(Clive) * W. Recover P from the two matrices Oblivion
// uploaded, then compose the same world geometry through the capture camera:
// Mreflection = P * inverse(Ccapture) * W.
inline bool BuildWaterWorldProjectionMatrix(const WaterMatrix& projected,
                                             const WaterMatrix& world,
                                             WaterMatrix& output) {
	WaterMatrix inverseWorld{};
	if (!InvertWaterMatrix(world, inverseWorld)) return false;
	output = MultiplyWaterMatrix(projected, inverseWorld);
	for (unsigned row = 0; row < 4; ++row)
		for (unsigned column = 0; column < 4; ++column)
			if (!std::isfinite(output.m[row][column])) return false;
	return true;
}

inline bool BuildReprojectedWaterMatrix(const WaterMatrix& currentMvp,
                                        const WaterMatrix& world,
                                        const NiTransform& liveCamera,
                                        const NiTransform& captureCamera,
                                        WaterMatrix& output) {
	WaterMatrix inverseWorld{}, inverseCapture{};
	if (!InvertWaterMatrix(world, inverseWorld) ||
	    !InvertWaterMatrix(CameraWorldMatrix(captureCamera), inverseCapture)) {
		return false;
	}
	const WaterMatrix projection = MultiplyWaterMatrix(
		MultiplyWaterMatrix(currentMvp, inverseWorld),
		CameraWorldMatrix(liveCamera));
	output = MultiplyWaterMatrix(MultiplyWaterMatrix(projection, inverseCapture), world);
	for (unsigned row = 0; row < 4; ++row)
		for (unsigned column = 0; column < 4; ++column)
			if (!std::isfinite(output.m[row][column])) return false;
	return true;
}

// Full water MVP used by the stable shader. Keeping this name separate from
// the camera-only projection helper makes it explicit that the result may be
// stored and replayed without rebuilding it from a later WorldMat.
inline bool BuildStableWaterCaptureMvp(const WaterMatrix& currentMvp,
                                       const WaterMatrix& world,
                                       const NiTransform& liveCamera,
                                       const NiTransform& captureCamera,
                                       WaterMatrix& output) {
	return BuildReprojectedWaterMatrix(currentMvp, world, liveCamera,
	                                   captureCamera, output);
}

inline bool CopyStoredWaterCaptureMvp(const WaterMatrix& stored,
                                      WaterMatrix& output) {
	for (unsigned row = 0; row < 4; ++row)
		for (unsigned column = 0; column < 4; ++column)
			if (!std::isfinite(stored.m[row][column])) return false;
	output = stored;
	return true;
}

inline bool CanStoreWaterCaptureMvp(UInt32 count, UInt32 capacity) {
	return capacity != 0 && count < capacity;
}

inline bool CanReplayWaterCaptureMvp(UInt32 cursor, UInt32 count) {
	return cursor < count;
}

inline bool PatchWaterVertexShader(UInt32* code, UInt32 dwordCount) {
	if (code == nullptr) return false;
	const UInt32 patterns[6][5] = {
		{0x00000005,0x800F0000,0x80FF0001,0xA0E40003,0},
		{0x00000004,0xE00F0002,0xA0E40000,0x80FF0001,0x80E40000},
		{0x00000004,0xE00F0003,0xA0E40001,0x80FF0001,0x80E40000},
		{0x00000004,0xE00F0004,0xA0E40002,0x80FF0001,0x80E40000},
		{0x00000001,0xE00F0005,0xA0E40003,0,0},
		{0x00000001,0xE00F0000,0x90E40000,0,0},
	};
	const UInt32 lengths[6] = {4,5,5,5,3,3};
	const UInt32 sourceSlots[6] = {3,2,2,2,2,2};
	const UInt32 replacements[6] = {
		0xA0E40003,0xA0E4000D,0xA0E4000E,0xA0E4000F,0xA0E40010,0x90E40000,
	};
	UInt32 locations[6]{};
	for (unsigned pattern = 0; pattern < 6; ++pattern) {
		unsigned matches = 0;
		for (UInt32 at = 0; at + lengths[pattern] <= dwordCount; ++at) {
			bool same = true;
			for (UInt32 i = 0; i < lengths[pattern]; ++i)
				if (code[at + i] != patterns[pattern][i]) { same = false; break; }
			if (same) { locations[pattern] = at; ++matches; }
		}
		if (matches != 1) return false;
	}
	for (unsigned pattern = 0; pattern < 6; ++pattern)
		code[locations[pattern] + sourceSlots[pattern]] = replacements[pattern];
	return true;
}


// The stable water pixel shader receives the original local water vertex in
// oT0 and projects it with a stored full capture MVP. A camera-only matrix
// multiplied by the current WorldMat is unsafe here: the game's water matrix
// can be camera-relative, so that recomposition moves when the HMD moves.
// Keeping the full capture MVP from the first eye makes the reflection
// coordinate independent of the second eye's WorldMat. oT1 remains the
// vanilla world/fresnel input. oT2-oT5 are still patched for the diagnostic
// vertex path, and oPos.w stays live so rasterization uses the HMD view.

inline bool PatchWaterVertexShaderNative(UInt32* code, UInt32 dwordCount) {
	if (code == nullptr) return false;
	const UInt32 patterns[6][5] = {
		{0x00000005,0x800F0000,0x80FF0001,0xA0E40003,0},
		{0x00000004,0xE00F0002,0xA0E40000,0x80FF0001,0x80E40000},
		{0x00000004,0xE00F0003,0xA0E40001,0x80FF0001,0x80E40000},
		{0x00000004,0xE00F0004,0xA0E40002,0x80FF0001,0x80E40000},
		{0x00000001,0xE00F0005,0xA0E40003,0,0},
		{0x00000001,0xE00F0000,0x90E40000,0,0},
	};
	const UInt32 lengths[6] = {4,5,5,5,3,3};
	const UInt32 sourceSlots[6] = {3,2,2,2,2,2};
	// r0 is the common half-W texture-coordinate bias, not oPos.w.
	// It must use capture W together with the capture X/Y/Z rows below.
	// The separate dp4 oPos.w, c3, v0 remains untouched for live rasterization.
	const UInt32 replacements[6] = {0xA0E40010,0xA0E4000D,0xA0E4000E,0xA0E4000F,0xA0E40010,0x90E40000};
	UInt32 locations[6]{};
	for (unsigned pattern = 0; pattern < 6; ++pattern) {
		unsigned matches = 0;
		for (UInt32 at = 0; at + lengths[pattern] <= dwordCount; ++at) {
			bool same = true;
			for (UInt32 i = 0; i < lengths[pattern]; ++i)
				if (code[at + i] != patterns[pattern][i]) { same = false; break; }
			if (same) { locations[pattern] = at; ++matches; }
		}
		if (matches != 1) return false;
	}
	for (unsigned pattern = 0; pattern < 6; ++pattern)
		code[locations[pattern] + sourceSlots[pattern]] = replacements[pattern];
	return true;
}

inline bool CanUseWaterReprojection(bool shadersReady, bool shadersRefused,
                                    bool captureHookReady) {
	return shadersReady && !shadersRefused && captureHookReady;
}

// Oblivion renders its own reflection target as a nested scene before the
// main world pass. That subpass is not replayed for the second stereo eye, so
// its water draws must not enter the main-pass capture queue or use the VR
// replacement shader.
inline bool ShouldBypassWaterReprojectionInReflectionSubpass(bool inSubpass) {
	return inSubpass;
}

void PrepareWaterReprojection(void* device);
void* SelectWaterVertexShader(void* device, void* requested, WaterReflectionMode mode);
bool BuildWaterReprojectionConstants(UInt32 startRegister, const float* data,
                                     UInt32 vector4fCount, float output[16]);
bool WaterReprojectionReady();
void SetWaterReflectionHookReady(bool ready);
bool GetWaterReprojectionSnapshot(WaterReprojectionSnapshot& snapshot);
void SetWaterReflectionCapture(const NiTransform& transform, bool stable);
void SetWaterReflectionCaptureSlot(unsigned slot, const NiTransform& transform, bool stable);
void SetWaterReflectionSubpass(bool active);
bool IsWaterReflectionSubpass();
void SetWaterCaptureProjectionScale(float horizontal, float vertical);
void SetWaterStereoPass(WaterStereoPass pass);
WaterStereoPass GetWaterStereoPass();
void NoteWaterReflectionRendered();
void NoteWaterReflectionReused();
bool WaterReflectionRenderedThisStereoPair();
bool DumpWaterShaderBinary(void* shader, const char* fileName);

}  // namespace obvr::render
