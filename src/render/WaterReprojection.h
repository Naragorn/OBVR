#pragma once

#include <cmath>

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::render {

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

inline bool UsesCyclopeanWaterCapture(WaterReflectionMode mode) {
	return mode == WaterReflectionMode::CyclopeanCapture ||
	       mode == WaterReflectionMode::Reprojected;
}

inline bool UsesWaterReprojectionShader(WaterReflectionMode mode) {
	return mode == WaterReflectionMode::CyclopeanCapture ||
	       mode == WaterReflectionMode::Reprojected;
}

inline bool ShouldRenderWaterReflection(bool stable, WaterStereoPass pass) {
	return !stable || pass != WaterStereoPass::Second;
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

struct WaterMatrix {
	float m[4][4]{};
};

struct WaterReprojectionSnapshot {
	WaterMatrix current{};
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
	const UInt32 replacements[6] = {0xA0E40003,0xA0E4000D,0xA0E4000E,0xA0E4000F,0xA0E40010,0x90E40000};
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
bool DumpWaterShaderBinary(void* shader, const char* fileName);

}  // namespace obvr::render
