#include "render/CullingHook.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "render/CullingSync.h"

namespace obvr::render {
namespace {

// Two complete, position-independent instructions at 0x0070E0A0:
//   push -1
//   push 0x009AEFA8
const UInt8 kCullingEntry[addr::kCullingProcessProcessEntryLength] = {
	0x6A, 0xFF, 0x68, 0xA8, 0xEF, 0x9A, 0x00,
};

// NiCullingProcess::Process is __thiscall with three stack arguments. The
// dead EDX slot makes this free __fastcall function byte-compatible with it.
using CullingProcessFn =
	void(__fastcall*)(void* self, void* unusedEdx, NiAVObject* camera,
	                  NiAVObject* scene, void* visibleSet);

CullingProcessFn g_original = nullptr;
CullingFrameSync g_sync;
UInt32 g_reused = 0;
UInt32 g_mismatched = 0;
UInt32 g_exhausted = 0;
UInt32 g_dropped = 0;

void __fastcall HookedCullingProcess(void* self, void* unusedEdx,
	                                  NiAVObject* camera, NiAVObject* scene,
	                                  void* visibleSet) {
	const NiPoint3 current = camera != nullptr ? camera->worldTransform.pos : NiPoint3{};
	NiPoint3 replacement{};
	const CullingSyncResult result = g_sync.Visit(camera, current, replacement);

	if (result == CullingSyncResult::Reused && camera != nullptr) {
		// Only the position that builds the frustum planes is shared. Restoring
		// it before returning leaves every later render operation on its real
		// eye camera.
		const NiPoint3 saved = camera->worldTransform.pos;
		camera->worldTransform.pos = replacement;
		g_original(self, unusedEdx, camera, scene, visibleSet);
		camera->worldTransform.pos = saved;
		++g_reused;
		return;
	}

	if (result == CullingSyncResult::CameraMismatch) {
		++g_mismatched;
	} else if (result == CullingSyncResult::NoCapturedCall) {
		++g_exhausted;
	} else if (result == CullingSyncResult::CaptureFull) {
		++g_dropped;
	}
	g_original(self, unusedEdx, camera, scene, visibleSet);
}

}  // namespace

bool InstallCullingHook() {
	if (g_original != nullptr) {
		return true;
	}
	if (!mem::Verify(addr::kCullingProcessProcess, kCullingEntry,
	                 sizeof(kCullingEntry))) {
		OBVR_LOG("Culling: bytes at %08X differ, the two eyes keep separate visible sets",
		         addr::kCullingProcessProcess);
		mem::ReportForeignCode("Culling", addr::kCullingProcessProcess);
		return false;
	}

	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Culling: no executable memory for the process trampoline");
		return false;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = mem::BuildEntryTrampoline(
		trampoline, kTrampolineCapacity, trampolineAddress,
		addr::kCullingProcessProcess, kCullingEntry, sizeof(kCullingEntry));
	if (trampolineSize == 0) {
		OBVR_LOG("Culling: the process trampoline does not fit");
		return false;
	}

	UInt8 patch[sizeof(kCullingEntry)];
	const UInt32 patchSize = mem::BuildEntryPatch(
		patch, sizeof(patch), addr::kCullingProcessProcess,
		reinterpret_cast<UInt32>(&HookedCullingProcess), sizeof(kCullingEntry));
	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Culling: the process patch has unexpected length %u", patchSize);
		return false;
	}

	g_original = reinterpret_cast<CullingProcessFn>(trampoline);
	if (!mem::SafeWrite(addr::kCullingProcessProcess, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Culling: SafeWrite to %08X failed, visible sets stay separate",
		         addr::kCullingProcessProcess);
		return false;
	}

	OBVR_LOG("Culling: process hooked at %08X, trampoline at %08X - both eyes now cull "
	         "from the first eye's position",
	         addr::kCullingProcessProcess, trampolineAddress);
	return true;
}

bool IsCullingHooked() { return g_original != nullptr; }

void BeginCullingCapture() {
	if (g_original == nullptr) {
		return;
	}
	g_reused = 0;
	g_mismatched = 0;
	g_exhausted = 0;
	g_dropped = 0;
	g_sync.BeginCapture();
}

void PauseCullingCapture() {
	if (g_original != nullptr) {
		g_sync.End();
	}
}

void BeginCullingReplay() {
	if (g_original != nullptr) {
		g_sync.BeginReplay();
	}
}

void EndCullingSync(UInt32 sceneCall) {
	if (g_original == nullptr) {
		return;
	}
	g_sync.End();
	if (sceneCall <= 3 || sceneCall % 120 == 0) {
		OBVR_LOG("Culling sync at scene call %u: captured %u, replayed %u, reused %u, "
		         "camera mismatches %u, replay overflow %u, capture overflow %u",
		         sceneCall, g_sync.CapturedCount(), g_sync.ReplayCount(), g_reused,
		         g_mismatched, g_exhausted, g_dropped);
	}
}

}  // namespace obvr::render
