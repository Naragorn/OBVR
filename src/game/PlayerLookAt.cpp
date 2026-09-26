#include "game/PlayerLookAt.h"

#include "core/Log.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

bool g_wanted = false;
bool g_valid = false;
NiPoint3 g_eyes{0.0f, 0.0f, 0.0f};
bool g_reported = false;

using LookAtFn = NiPoint3*(__thiscall*)(void* self, NiPoint3* out);

// thiscall(self, out), ret 4: a fastcall with the stack argument after edx
// is called and returns the same way.
NiPoint3* __fastcall OnLookAt(void* self, void* /*edx*/, NiPoint3* out) {
	const bool thirdPerson =
		*reinterpret_cast<const UInt8*>(reinterpret_cast<UInt32>(self) +
		                                kPlayerThirdPersonOffset) != 0;
	if (out != nullptr && LookAtFromEyes(g_wanted, g_valid, thirdPerson)) {
		*out = LookAtPointFromEyes(g_eyes);
		if (!g_reported) {
			g_reported = true;
			OBVR_LOG("NPC look: the player is looked at from the headset's eyes, not Camera01");
		}
		return out;
	}
	return reinterpret_cast<LookAtFn>(kPlayerLookAtOriginal)(self, out);
}

}  // namespace

void InstallPlayerLookAt() {
	const UInt32 original = kPlayerLookAtOriginal;
	if (!mem::Verify(kPlayerLookAtSlot, reinterpret_cast<const UInt8*>(&original),
	                 sizeof(original))) {
		OBVR_LOG("NPC look: the player's look-at slot %08X does not hold %08X - left alone",
		         kPlayerLookAtSlot, kPlayerLookAtOriginal);
		return;
	}
	const UInt32 replacement = reinterpret_cast<UInt32>(&OnLookAt);
	if (!mem::SafeWrite(kPlayerLookAtSlot, &replacement, sizeof(replacement))) {
		OBVR_LOG("NPC look: could not write the player's look-at slot %08X", kPlayerLookAtSlot);
		return;
	}
	OBVR_LOG("NPC look: the player's look-at point (slot %08X) answers the headset's eyes in "
	         "first person",
	         kPlayerLookAtSlot);
}

void SetPlayerLookAtEyes(bool wanted, bool valid, const NiPoint3& eyes) {
	g_wanted = wanted;
	g_valid = valid;
	g_eyes = eyes;
}

}  // namespace obvr::game
