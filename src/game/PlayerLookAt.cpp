#include "game/PlayerLookAt.h"
#include "game/DialogFocus.h"

#include "core/Log.h"
#include "core/Memory.h"

namespace obvr::game {
namespace {

bool g_wanted = false;
bool g_valid = false;
NiPoint3 g_eyes{0.0f, 0.0f, 0.0f};
bool g_reported = false;
DialogFocus g_dialog;

using LookAtFn = NiPoint3*(__thiscall*)(void* self, NiPoint3* out);

// thiscall(self, out), ret 4: a fastcall with the stack argument after edx
// is called and returns the same way.
NiPoint3* __fastcall OnLookAt(void* self, void* /*edx*/, NiPoint3* out) {
	const bool thirdPerson =
		*reinterpret_cast<const UInt8*>(reinterpret_cast<UInt32>(self) +
		                                kPlayerThirdPersonOffset) != 0;
	// The eyes the conversation holds once its menu is up, the live ones
	// before (DialogFocus::UseEyes).
	if (out != nullptr && g_dialog.UseEyes(g_wanted, thirdPerson, g_valid)) {
		*out = g_dialog.active && g_dialog.sawMenu ? g_dialog.eyes : LookAtPointFromEyes(g_eyes);
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
	if (!wanted) g_dialog.eyesValid = false;
	// The live eyes every frame; the eyes the conversation holds are its own
	// copy, refreshed from these until its menu is up (DialogFocus).
	g_valid = wanted && valid && DialogPositionValid(eyes);
	g_eyes = eyes;
	if (g_dialog.TakesLiveEyes() && g_valid) {
		g_dialog.eyes = eyes;
		g_dialog.eyesValid = true;
	}
}

void ObservePlayerDialog(bool hasActor, bool speakerValid, const NiPoint3& speaker) {
	const bool opening = hasActor && !g_dialog.active;
	g_dialog.Observe(hasActor, g_wanted && g_valid, g_eyes, speakerValid, speaker);
	if (opening) {
		OBVR_LOG("Dialogue focus: eyes frozen=%d at %.2f/%.2f/%.2f, speaker valid=%d at %.2f/%.2f/%.2f",
		         g_dialog.eyesValid ? 1 : 0, static_cast<double>(g_dialog.eyes.x),
		         static_cast<double>(g_dialog.eyes.y), static_cast<double>(g_dialog.eyes.z),
		         g_dialog.speakerValid ? 1 : 0, static_cast<double>(speaker.x),
		         static_cast<double>(speaker.y), static_cast<double>(speaker.z));
	}
}

void StepPlayerDialog(bool menuIsUp) { g_dialog.Step(menuIsUp); }
bool PlayerDialogActive() { return g_dialog.active; }

bool ReadDialogSpeaker(NiPoint3& speaker) {
	if (!g_dialog.active || !g_dialog.speakerValid) return false;
	speaker = g_dialog.speaker;
	return true;
}
bool ReadDialogEyes(NiPoint3& eyes) {
	if (!g_dialog.active || !g_dialog.eyesValid) return false;
	eyes = g_dialog.eyes;
	return true;
}

}  // namespace obvr::game
