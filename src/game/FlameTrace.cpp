#include "game/FlameTrace.h"

#include <intrin.h>

#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

constexpr UInt32 kFlamesOff = 0x004DE1C0;
// Every `call 0x004DE1C0` in Oblivion.exe 1.2.0.416 (objdump, 2026-11-18).
constexpr UInt32 kSites[] = {0x004787A6, 0x004DE2EE, 0x004E12A6, 0x004E2565, 0x004E41F0, 0x00505057};

UInt32 __cdecl FlamesOffTraced(UInt32 node) {
	const UInt32 back = reinterpret_cast<UInt32>(_ReturnAddress());
	using FlamesOffFn = UInt32(__cdecl*)(UInt32 node);
	const UInt32 result = reinterpret_cast<FlamesOffFn>(kFlamesOff)(node);
	static UInt32 s_lines = 80;
	if (s_lines > 0) {
		--s_lines;
		OBVR_LOG("Flame trace: the flames off on node %08X, called from %08X (the call at %08X) - answered %u", node,
		         back, back - 5, result & 0xFF);
	}
	return result;
}

}  // namespace

void InstallFlameOffTrace() {
	static bool installed = false;
	if (installed) {
		return;
	}
	installed = true;
	UInt32 done = 0;
	for (UInt32 site : kSites) {
		const UInt32 d = mem::CallRelativeDisplacement(site, kFlamesOff);
		const UInt8 expected[5] = {0xE8, static_cast<UInt8>(d & 0xFF), static_cast<UInt8>((d >> 8) & 0xFF),
		                           static_cast<UInt8>((d >> 16) & 0xFF), static_cast<UInt8>((d >> 24) & 0xFF)};
		if (!mem::Verify(site, expected, sizeof(expected))) {
			OBVR_LOG("Flame trace: the call at %08X is not `call %08X` - left alone", site, kFlamesOff);
			continue;
		}
		UInt8 patch[5];
		if (mem::BuildCallSitePatch(patch, sizeof(patch), site, reinterpret_cast<UInt32>(&FlamesOffTraced)) ==
		        sizeof(patch) &&
		    mem::SafeWrite(site, patch, sizeof(patch))) {
			++done;
		}
	}
	OBVR_LOG("Flame trace: %u of %u calls of the flames off (%08X) traced", done,
	         static_cast<UInt32>(sizeof(kSites) / sizeof(kSites[0])), kFlamesOff);
}

}  // namespace obvr::game
