#pragma once

#include "core/SelfCheckLogic.h"

// What SteamVR said about the controllers and the headset, handed from the
// OpenVR backend (vr/OpenVRControllerKind.cpp, any thread) to the install
// self-check (game/SelfCheck.cpp, the game's loop). Plain values under a
// spin lock; nothing here reaches the game.
namespace obvr::selfcheck {

// A hand's Prop_ControllerType_String ("knuckles" is the Index).
void NoteController(bool rightHand, const char* type);
// The headset's Prop_ModelNumber_String and Prop_ManufacturerName_String.
void NoteHeadset(const char* model, const char* manufacturer);

struct SteamVrNotes {
	Controller right = Controller::Unknown;
	Controller left = Controller::Unknown;
	bool hmdRead = false;
	bool hmdQuestLike = false;
};
SteamVrNotes ReadSteamVrNotes();

// "knuckles" the Index, any other type another controller, none unknown
// (the rule of vr::ControllerKindFromType).
inline Controller ControllerFromType(const char* type) {
	if (type == nullptr || type[0] == '\0') {
		return Controller::Unknown;
	}
	return std::strcmp(type, "knuckles") == 0 ? Controller::Index : Controller::Other;
}

}  // namespace obvr::selfcheck
