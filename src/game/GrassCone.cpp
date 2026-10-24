#include "game/GrassCone.h"

#include "core/AddressSpace.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/FirstPersonHide.h"
#include "game/GameTypes.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// FUN_004EBDA0: cdecl, the camera's position, its heading and the cone; the
// caller pops the seven arguments (0x0066C170: add esp,0x1C).
using GrassBuildFn = void(__cdecl*)(float x, float y, float z, float dx, float dy, float dz, float cone);

GrassBuildFn g_original = nullptr;
bool g_allAround = false;
GrassBuildStats g_stats;
UInt32 g_buildLines = 40;

// The patches under the Grass node: its children's children (each child a
// patch group of the build's).
UInt32 CountGrassPatches() {
	const NiAVObject* const node = *reinterpret_cast<NiAVObject* const*>(kGrassNode);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node))) {
		return 0;
	}
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(node, count);
	if (children == nullptr) {
		return 0;
	}
	UInt32 patches = 0;
	for (UInt32 i = 0; i < count; ++i) {
		UInt32 inner = 0;
		if (children[i] != nullptr && NodeChildren(children[i], inner) != nullptr) {
			patches += inner;
		} else if (children[i] != nullptr) {
			++patches;
		}
	}
	return patches;
}

void __cdecl HookedGrassBuild(float x, float y, float z, float dx, float dy, float dz, float cone) {
	const float used = GrassConeFor(g_allAround, cone);
	const long long before = ReadPerformanceCounter();
	g_original(x, y, z, dx, dy, dz, used);
	const long long after = ReadPerformanceCounter();
	const long long frequency = ReadPerformanceFrequency();
	const double ms = frequency > 0 ? static_cast<double>(after - before) * 1000.0 / static_cast<double>(frequency) : 0.0;
	++g_stats.builds;
	g_stats.totalMs += ms;
	g_stats.maxMs = ms > g_stats.maxMs ? ms : g_stats.maxMs;
	g_stats.lastEngineCone = cone;
	g_stats.lastUsedCone = used;
	if (g_buildLines > 0 || g_stats.builds % 100 == 0) {
		if (g_buildLines > 0) {
			--g_buildLines;
		}
		g_stats.patches = CountGrassPatches();
		const float heading = math::Atan2(dx, dy) * 57.2957795f;
		OBVR_LOG("Grass: build %u at %.0f %.0f heading %.1f deg - the engine's cone %.3f (%.1f deg either side), "
		         "used %.3f (%.1f), %.2f ms, %u patches under the Grass node",
		         g_stats.builds, static_cast<double>(x), static_cast<double>(y), static_cast<double>(heading),
		         static_cast<double>(cone), static_cast<double>(GrassConeDegrees(cone)), static_cast<double>(used),
		         static_cast<double>(GrassConeDegrees(used)), ms, g_stats.patches);
	}
}

}  // namespace

void SetGrassAllAround(bool allAround) {
	g_allAround = allAround;
}

GrassBuildStats ReadGrassBuildStats() {
	GrassBuildStats stats = g_stats;
	stats.patches = g_original != nullptr ? CountGrassPatches() : 0;
	return stats;
}

void InstallGrassCone() {
	if (g_original != nullptr) {
		return;
	}
	if (!mem::Verify(kGrassBuild, kGrassBuildEntry, sizeof(kGrassBuildEntry))) {
		OBVR_LOG("Grass: the bytes at %08X are not the grass build as read - left alone, the grass keeps the "
		         "engine's view cone",
		         kGrassBuild);
		mem::ReportForeignCode("Grass cone", kGrassBuild);
		return;
	}
	constexpr UInt32 kTrampolineCapacity = 16;
	auto* const trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Grass: no executable memory for the trampoline");
		return;
	}
	const UInt32 trampolineSize =
		mem::BuildEntryTrampoline(trampoline, kTrampolineCapacity, reinterpret_cast<UInt32>(trampoline), kGrassBuild,
		                          kGrassBuildEntry, sizeof(kGrassBuildEntry));
	UInt8 patch[sizeof(kGrassBuildEntry)];
	const UInt32 patchSize = mem::BuildEntryPatch(patch, sizeof(patch), kGrassBuild,
	                                              reinterpret_cast<UInt32>(&HookedGrassBuild), sizeof(kGrassBuildEntry));
	if (trampolineSize == 0 || patchSize != sizeof(patch)) {
		OBVR_LOG("Grass: the detour's bytes do not fit");
		return;
	}
	g_original = reinterpret_cast<GrassBuildFn>(trampoline);
	if (!mem::SafeWrite(kGrassBuild, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Grass: the write to %08X failed - left alone", kGrassBuild);
		return;
	}
	OBVR_LOG("Grass: the grass build (%08X) is watched - with a headset and Render.GrassAllAround it is handed a "
	         "cone all around the eyes",
	         kGrassBuild);
}

}  // namespace obvr::game
