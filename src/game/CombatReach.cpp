#include "game/CombatReach.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

bool g_checked = false;
bool g_usable = false;
float g_gameOwn = 0.0f;
float g_written = -1.0f;

// The Setting's name, kept after its value (xOBSE GameAPI.h: SettingInfo is
// vtable, data, name; the value is the data).
bool NamedCombatDistance() {
	const char* const name = *reinterpret_cast<const char* const*>(kCombatDistanceValue + 4);
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(name))) {
		return false;
	}
	const char kWanted[] = "fCombatDistance";
	for (UInt32 i = 0; i < sizeof(kWanted); ++i) {
		if (name[i] != kWanted[i]) {
			return false;
		}
	}
	return true;
}

}  // namespace

void StepCombatReach(bool fullVr, float setting) {
	if (!g_checked) {
		g_checked = true;
		g_usable = NamedCombatDistance();
		g_gameOwn = *reinterpret_cast<const float*>(kCombatDistanceValue);
		g_usable = g_usable && g_gameOwn > 0.0f && g_gameOwn < 1000.0f;
		OBVR_LOG("Combat reach: %s (the game's own %.1f)",
		         g_usable ? "fCombatDistance found at 00B36F20" : "the setting at 00B36F20 is not fCombatDistance - left alone",
		         static_cast<double>(g_gameOwn));
	}
	if (!g_usable) {
		return;
	}
	float* const value = reinterpret_cast<float*>(kCombatDistanceValue);
	// Changed by someone else since (the data files loading after the first
	// frames, a mod's script): that is the game's own now.
	if (g_written >= 0.0f && *value != g_written && *value > 0.0f && *value < 1000.0f) {
		g_gameOwn = *value;
		OBVR_LOG("Combat reach: fCombatDistance set to %.1f by the game - taken as its own",
		         static_cast<double>(g_gameOwn));
	}
	const float want = CombatReachFor(fullVr, setting, g_gameOwn);
	if (*value != want) {
		*value = want;
	}
	if (want != g_written) {
		g_written = want;
		OBVR_LOG("Combat reach: fCombatDistance %.1f%s", static_cast<double>(want),
		         want == g_gameOwn ? " (the game's own)" : " - fighters close in to strike");
	}
}

}  // namespace obvr::game
