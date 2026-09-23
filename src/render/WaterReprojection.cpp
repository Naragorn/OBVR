#include "render/WaterReprojection.h"

#include "camera/CameraHook.h"
#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "render/D3D9Types.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"

namespace obvr::render {
namespace {

constexpr UInt32 kWaterShaderPointer = 0x00B45DCC;
constexpr UInt32 kVertexArrayOffset = 0xBC;
constexpr UInt32 kVertexCount = 2;
constexpr UInt32 kVertexShaderHandleOffset = 0x30;
constexpr UInt32 kShaderGetFunction = 4;
constexpr UInt32 kUnknownRelease = 2;
constexpr UInt32 kMaxShaderBytes = 4096;

using GetFunctionFn = SInt32(__stdcall*)(void* self, void* data, UInt32* size);
using ReleaseFn = UInt32(__stdcall*)(void* self);

void* g_device = nullptr;
void* g_original[kVertexCount]{};
void* g_nativeReplacement[kVertexCount]{};
bool g_active = false;
bool g_ready = false;
bool g_reported = false;
bool g_refused = false;
float g_constants[8][4]{};
UInt32 g_registerMask = 0;
NiTransform g_capture[kWaterReflectionCaptureCount]{};
bool g_captureStable[kWaterReflectionCaptureCount]{};
bool g_captureHookReady = false;
bool g_constantsReported = false;
WaterReprojectionSnapshot g_snapshot{};
bool g_snapshotValid = false;
WaterReflectionMode g_mode = WaterReflectionMode::Vanilla;
WaterStereoPass g_stereoPass = WaterStereoPass::Single;
bool g_inReflectionSubpass = false;
constexpr UInt32 kMaxStoredWaterDraws = 2048;
WaterMatrix g_captureWaterMvp[kMaxStoredWaterDraws][kWaterReflectionCaptureCount]{};
UInt32 g_captureWaterMvpCount = 0;
UInt32 g_captureWaterMvpReplay = 0;
UInt32 g_captureProjectionSerial = 0;
UInt32 g_reflectionRenderSerial = 0;
UInt32 g_reflectionReuseSerial = 0;
bool g_reflectionRenderedThisStereoPair = false;
float g_captureProjectionHorizontal=1.0f;
float g_captureProjectionVertical=1.0f;

bool WriteShaderBinary(void* shader, const char* fileName) {
	if (shader == nullptr || fileName == nullptr) return false;
	auto getFunction = d3d9::Method<GetFunctionFn>(shader, kShaderGetFunction);
	UInt32 byteCount = 0;
	if (getFunction == nullptr || getFunction(shader, nullptr, &byteCount) < 0 ||
	    byteCount == 0 || byteCount > kMaxShaderBytes) return false;
	UInt8 bytes[kMaxShaderBytes]{};
	if (getFunction(shader, bytes, &byteCount) < 0) return false;
	char path[512]{};
	if (!platform::BuildGamePath(fileName, path, sizeof(path))) return false;
	HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
	                          FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == InvalidHandle()) return false;
	DWORD written = 0;
	const bool ok = WriteFile(file, bytes, byteCount, &written, nullptr) != 0 &&
	                written == byteCount;
	CloseHandle(file);
	return ok;
}

void ReleaseReplacements() {
	for (unsigned i = 0; i < kVertexCount; ++i) {
		if (g_nativeReplacement[i] != nullptr) {
			auto release = d3d9::Method<ReleaseFn>(g_nativeReplacement[i], kUnknownRelease);
			if (release != nullptr) release(g_nativeReplacement[i]);
		}
		g_nativeReplacement[i] = nullptr;
		g_original[i] = nullptr;
	}
	g_ready = false;
	g_active = false;
}

bool ReadOriginalHandles(void* handles[kVertexCount]) {
	const UInt32 water = *reinterpret_cast<const UInt32*>(kWaterShaderPointer);
	if (!mem::LooksLikeObjectAddress(water)) return false;
	for (unsigned i = 0; i < kVertexCount; ++i) {
		const UInt32 wrapper =
			*reinterpret_cast<const UInt32*>(water + kVertexArrayOffset + i * 4);
		if (!mem::LooksLikeObjectAddress(wrapper)) return false;
		handles[i] =
			*reinterpret_cast<void* const*>(wrapper + kVertexShaderHandleOffset);
		if (handles[i] == nullptr) return false;
	}
	return true;
}

bool SameOriginalHandles(void* const handles[kVertexCount]) {
	for (unsigned i = 0; i < kVertexCount; ++i)
		if (g_original[i] != handles[i]) return false;
	return true;
}

void StoreOriginalHandles(void* const handles[kVertexCount]) {
	for (unsigned i = 0; i < kVertexCount; ++i) g_original[i] = handles[i];
}

bool CreateNativeReplacement(unsigned index) {
	UInt32 byteCount = 0;
	auto getFunction = d3d9::Method<GetFunctionFn>(g_original[index], kShaderGetFunction);
	if (getFunction == nullptr ||
	    getFunction(g_original[index], nullptr, &byteCount) < 0 ||
	    byteCount == 0 || byteCount > kMaxShaderBytes || byteCount % 4 != 0) {
		return false;
	}
	UInt8 codeBytes[kMaxShaderBytes]{};
	if (getFunction(g_original[index], codeBytes, &byteCount) < 0 ||
	    !PatchWaterVertexShaderNative(reinterpret_cast<UInt32*>(codeBytes), byteCount / 4)) {
		return false;
	}
	auto create = d3d9::Method<d3d9::CreateVertexShaderFn>(
		g_device, d3d9::kDeviceCreateVertexShader);
	return create != nullptr &&
	       create(g_device, reinterpret_cast<const UInt32*>(codeBytes),
	              &g_nativeReplacement[index]) >= 0 &&
	       g_nativeReplacement[index] != nullptr;
}

int OriginalIndex(void* shader) {
	for (unsigned i = 0; i < kVertexCount; ++i)
		if (g_original[i] == shader) return static_cast<int>(i);
	return -1;
}

}  // namespace

void PrepareWaterReprojection(void* device) {
	if (device == nullptr) return;
	void* current[kVertexCount]{};
	const bool currentValid = ReadOriginalHandles(current);
	const bool deviceChanged = g_device != device;
	const bool identityMatches = currentValid && SameOriginalHandles(current);
	const WaterShaderLifecycleAction action = DecideWaterShaderLifecycle(
		deviceChanged, currentValid, identityMatches, g_ready, g_refused);
	if (action == WaterShaderLifecycleAction::KeepReady ||
	    action == WaterShaderLifecycleAction::KeepRefused) return;
	if (action == WaterShaderLifecycleAction::WaitForShaders) {
		ReleaseReplacements();
		g_device = device;
		g_refused = false;
		return;
	}
	ReleaseReplacements();
	g_device = device;
	g_refused = false;
	StoreOriginalHandles(current);
	if (GetConfig().vrTestSuite) {
		DumpWaterShaderBinary(g_original[0], "OBVR-WaterVS-0.bin");
		DumpWaterShaderBinary(g_original[1], "OBVR-WaterVS-1.bin");
	}
	for (unsigned i = 0; i < kVertexCount; ++i) {
		if (g_nativeReplacement[i] == nullptr && !CreateNativeReplacement(i)) {
			ReleaseReplacements();
			StoreOriginalHandles(current);
			g_refused = true;
			OBVR_LOG("Water reflection: stable reflection-coordinate shader did not match; stable water mode falls back to vanilla");
			return;
		}
	}
	g_ready = true;
	if (!g_reported) {
		g_reported = true;
		OBVR_LOG("Water reflection: verified original-pixel-path vertex shaders created");
	}
}

void* SelectWaterVertexShader(void* device, void* requested, WaterReflectionMode mode) {
	g_active = false;
	g_registerMask = 0;
	g_mode = mode;
	if (ShouldBypassWaterReprojectionInReflectionSubpass(g_inReflectionSubpass))
		return requested;
	if (!UsesWaterReprojectionShader(mode)) return requested;
	PrepareWaterReprojection(device);
	static unsigned unmatchedReports = 0;
	static unsigned selectedReports = 0;
	bool capturesStable = true;
	for (unsigned slot = 0; slot < kWaterReflectionCaptureCount; ++slot)
		capturesStable = capturesStable && g_captureStable[slot];
	if (!ShouldSelectStableWaterVertexShader(
			mode, g_inReflectionSubpass, g_ready, capturesStable)) {
		return requested;
	}
	const int index = OriginalIndex(requested);
	if (index < 0) {
		if (unmatchedReports < 4) {
			++unmatchedReports;
			OBVR_LOG("Water reflection: water vertex request %08X did not match either original shader",
			         reinterpret_cast<UInt32>(requested));
		}
		return requested;
	}
	g_active = true;
	void* selected = g_nativeReplacement[index];
	if (selectedReports < 4) {
		++selectedReports;
		OBVR_LOG("Water reflection: water vertex selected original=%08X replacement=%08X index=%d stable=%u",
		         reinterpret_cast<UInt32>(requested), reinterpret_cast<UInt32>(selected),
		         index, 1u);
	}
	return selected;
}

bool BuildWaterReprojectionConstants(UInt32 startRegister, const float* data,
                                     UInt32 vector4fCount, float output[16]) {
	bool capturesStable = true;
	for (unsigned slot = 0; slot < kWaterReflectionCaptureCount; ++slot)
		capturesStable = capturesStable && g_captureStable[slot];
	if (ShouldBypassWaterReprojectionInReflectionSubpass(g_inReflectionSubpass) ||
	    !g_active || !g_ready || !capturesStable || data == nullptr ||
	    vector4fCount == 0 || output == nullptr) return false;
	for (UInt32 i = 0; i < vector4fCount; ++i) {
		const UInt32 reg = startRegister + i;
		if (reg < startRegister || reg >= 8) continue;
		for (unsigned component = 0; component < 4; ++component)
			g_constants[reg][component] = data[i * 4 + component];
		g_registerMask |= 1u << reg;
	}
	if ((g_registerMask & 0xFFu) != 0xFFu) return false;
	NiTransform live{};
	const bool liveValid = camera::GetCurrentCameraWorldTransform(live);
	WaterMatrix current{}, world{}, projected[kWaterReflectionCaptureCount]{};
	for (unsigned row = 0; row < 4; ++row)
		for (unsigned column = 0; column < 4; ++column) {
			current.m[row][column] = g_constants[row][column];
			world.m[row][column] = g_constants[row + 4][column];
		}
	WaterMatrix absoluteWorld{};
	const bool worldValid = liveValid && RestoreWaterWorldOrigin(world, live.pos, absoluteWorld);
	if (worldValid) world = absoluteWorld;
	WaterMatrix reflection[kWaterReflectionCaptureCount]{};
	const bool storedValid = g_captureWaterMvpCount != 0;
	bool fallback = !worldValid;
	bool reusedCaptureProjection = false;
	const WaterProjectionPlan plan = ChooseWaterProjectionPlan(
		g_mode, g_stereoPass, storedValid);
	if (!fallback && plan == WaterProjectionPlan::ReuseStored) {
		if (!CanReplayWaterCaptureMvp(g_captureWaterMvpReplay, g_captureWaterMvpCount)) {
			fallback = true;
		} else {
			for (unsigned slot = 0; slot < kWaterReflectionCaptureCount; ++slot) {
				if (!CopyStoredWaterCaptureMvp(
						g_captureWaterMvp[g_captureWaterMvpReplay][slot],
						reflection[slot])) {
					fallback = true;
					break;
				}
				projected[slot] = reflection[slot];
			}
			if (!fallback) {
				++g_captureWaterMvpReplay;
				reusedCaptureProjection = true;
			}
		}
	} else if (!fallback && plan == WaterProjectionPlan::MissingStored) {
		fallback = true;
	} else if (!fallback) {
		WaterMatrix inverseWorld{};
		if (!InvertWaterMatrix(world, inverseWorld)) {
			fallback = true;
		} else {
			for (unsigned slot = 0; slot < kWaterReflectionCaptureCount; ++slot) {
				if (!BuildStableWaterCaptureMvp(current, world, live, g_capture[slot],
				                               projected[slot]) ||
				    !ScaleWaterProjectionRows(projected[slot],
				                              g_captureProjectionHorizontal,
				                              g_captureProjectionVertical)) {
					fallback = true;
				} else {
					reflection[slot] = projected[slot];
				}
			}
		}
		if (!fallback && plan == WaterProjectionPlan::CaptureAndStore) {
			if (!CanStoreWaterCaptureMvp(g_captureWaterMvpCount, kMaxStoredWaterDraws)) {
				fallback = true;
			} else {
				for (unsigned slot = 0; slot < kWaterReflectionCaptureCount; ++slot)
					g_captureWaterMvp[g_captureWaterMvpCount][slot] = projected[slot];
				++g_captureWaterMvpCount;
				++g_captureProjectionSerial;
			}
		}
	}
	if (fallback) {
		for (unsigned slot = 0; slot < kWaterReflectionCaptureCount; ++slot) {
			projected[slot] = current;
			reflection[slot] = current;
		}
	}
	for (unsigned row = 0; row < 4; ++row)
		for (unsigned column = 0; column < 4; ++column)
			output[row * 4 + column] = projected[1].m[row][column];
	g_snapshot.current = current;
	g_snapshot.world = world;
	g_snapshot.reprojected = projected[1];
	g_snapshot.live = live;
	g_snapshot.capture = g_capture[1];
	g_snapshot.captureProjectionSerial = g_captureProjectionSerial;
	g_snapshot.reflectionRenderSerial = g_reflectionRenderSerial;
	g_snapshot.reflectionReuseSerial = g_reflectionReuseSerial;
	g_snapshot.pass = g_stereoPass;
	g_snapshot.captureHorizontalScale = g_captureProjectionHorizontal;
	g_snapshot.captureVerticalScale = g_captureProjectionVertical;
	g_snapshot.reusedCaptureProjection = reusedCaptureProjection;
	g_snapshot.storedWaterDrawCount = g_captureWaterMvpCount;
	g_snapshot.replayedWaterDrawCount = g_captureWaterMvpReplay;
	g_snapshot.fallback = fallback;
	++g_snapshot.serial;
	g_snapshotValid = true;
	if (!g_constantsReported) {
		g_constantsReported = true;
		OBVR_LOG("Water reflection: original target projection matrix uploaded");
	}
	return true;
}

bool WaterReprojectionReady() {
	return CanUseWaterReprojection(g_ready, g_refused, g_captureHookReady);
}

void SetWaterReflectionHookReady(bool ready) { g_captureHookReady = ready; }

bool GetWaterReprojectionSnapshot(WaterReprojectionSnapshot& snapshot) {
	if (!g_snapshotValid) return false;
	snapshot = g_snapshot;
	return true;
}

void SetWaterReflectionCapture(const NiTransform& transform, bool stable) {
	for (unsigned slot = 0; slot < kWaterReflectionCaptureCount; ++slot) {
		g_capture[slot] = transform;
		g_captureStable[slot] = stable;
	}
}

void SetWaterReflectionCaptureSlot(unsigned slot, const NiTransform& transform, bool stable) {
	if (slot >= kWaterReflectionCaptureCount) return;
	g_capture[slot] = transform;
	g_captureStable[slot] = stable;
}

void SetWaterReflectionSubpass(bool active) {
	g_inReflectionSubpass = active;
	if (active) {
		// A nested reflection scene is not part of the first/second-eye draw
		// correspondence. Clear the active water shader state so any constants
		// it uploads cannot start or advance the main-pass queue.
		g_active = false;
		g_registerMask = 0;
	}
}

bool IsWaterReflectionSubpass() { return g_inReflectionSubpass; }

void SetWaterCaptureProjectionScale(float horizontal,float vertical) {
 g_captureProjectionHorizontal=horizontal;
 g_captureProjectionVertical=vertical;
}

void SetWaterStereoPass(WaterStereoPass pass) {
	g_stereoPass = pass;
	if (pass != WaterStereoPass::Second) {
		g_captureWaterMvpCount = 0;
		g_captureWaterMvpReplay = 0;
		g_reflectionRenderedThisStereoPair = false;
	}
}

WaterStereoPass GetWaterStereoPass() { return g_stereoPass; }

void NoteWaterReflectionRendered() {
	++g_reflectionRenderSerial;
	g_reflectionRenderedThisStereoPair = true;
}
void NoteWaterReflectionReused() { ++g_reflectionReuseSerial; }
bool WaterReflectionRenderedThisStereoPair() {
	return g_reflectionRenderedThisStereoPair;
}
bool DumpWaterShaderBinary(void* shader, const char* fileName) {
	return WriteShaderBinary(shader, fileName);
}

}  // namespace obvr::render
