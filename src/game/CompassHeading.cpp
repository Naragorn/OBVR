#include "game/CompassHeading.h"

#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

// HUDMainMenu's update reading the heading for the compass (CompassHeading.h).
constexpr UInt32 kHeadingRead = 0x005A6F05;
constexpr UInt32 kHeadingReadBytes = 16;
constexpr UInt8 kHeadingReadExpected[kHeadingReadBytes] = {0x8B, 0x0D, 0xC4, 0x33, 0xB3, 0x00, 0x8B, 0x11,
                                                           0x8B, 0x82, 0xE0, 0x01, 0x00, 0x00, 0xFF, 0xD0};
constexpr UInt32 kPlayerPointer = 0x00B333C4;
constexpr UInt32 kHeadingSlot = 0x1E0;

bool g_valid = false;
float g_heading = 0.0f;

using HeadingVirtual = float(__thiscall*)(void*);

// Called in place of `call [player vtable + 1E0h]`: no arguments, the heading
// in radians on st0, ebx/esi/edi/ebp kept (cdecl's own rules).
float __cdecl CompassHeading() {
	if (g_valid) {
		return g_heading;
	}
	void* const player = *reinterpret_cast<void**>(kPlayerPointer);
	if (player == nullptr) {
		return 0.0f;
	}
	HeadingVirtual* const vtable = *reinterpret_cast<HeadingVirtual**>(player);
	return vtable[kHeadingSlot / sizeof(void*)](player);
}

}  // namespace

bool InstallCompassHeading() {
	if (!mem::Verify(kHeadingRead, kHeadingReadExpected, kHeadingReadBytes)) {
		OBVR_LOG("Compass: the heading read at %08X is not the player's virtual +1E0 - the compass keeps the body's "
		         "heading",
		         kHeadingRead);
		mem::ReportForeignCode("Compass heading", kHeadingRead);
		return false;
	}
	UInt8 patch[kHeadingReadBytes];
	for (UInt8& b : patch) {
		b = 0x90;
	}
	if (mem::BuildCallSitePatch(patch, sizeof(patch), kHeadingRead, reinterpret_cast<UInt32>(&CompassHeading)) != 5 ||
	    !mem::SafeWrite(kHeadingRead, patch, sizeof(patch))) {
		OBVR_LOG("Compass: could not reroute the heading read at %08X", kHeadingRead);
		return false;
	}
	OBVR_LOG("Compass: the heading read at %08X goes through OBVR - the compass turns with the view", kHeadingRead);
	return true;
}

void SetCompassViewHeading(bool valid, float radians) {
	static bool s_logged = false;
	if (valid && !s_logged) {
		s_logged = true;
		OBVR_LOG("Compass: first view heading %.1f degrees", static_cast<double>(radians * math::kRadiansToDegrees));
	}
	g_valid = valid;
	g_heading = radians;
}

}  // namespace obvr::game
