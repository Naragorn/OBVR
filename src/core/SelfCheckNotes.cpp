#include "core/SelfCheckNotes.h"

#include <intrin.h>

namespace obvr::selfcheck {
namespace {

struct Notes {
	volatile long lock = 0;
	SteamVrNotes values;
};

Notes& TheNotes() {
	static Notes notes;
	return notes;
}

void Lock(Notes& n) {
	while (_InterlockedExchange(&n.lock, 1) != 0) {
		_mm_pause();
	}
}

void Unlock(Notes& n) { _InterlockedExchange(&n.lock, 0); }

}  // namespace

void NoteController(bool rightHand, const char* type) {
	const Controller c = ControllerFromType(type);
	Notes& n = TheNotes();
	Lock(n);
	(rightHand ? n.values.right : n.values.left) = c;
	Unlock(n);
}

void NoteHeadset(const char* model, const char* manufacturer) {
	const bool questLike = HmdLooksLikeQuest(model, manufacturer);
	Notes& n = TheNotes();
	Lock(n);
	n.values.hmdRead = true;
	n.values.hmdQuestLike = questLike;
	Unlock(n);
}

SteamVrNotes ReadSteamVrNotes() {
	Notes& n = TheNotes();
	Lock(n);
	const SteamVrNotes copy = n.values;
	Unlock(n);
	return copy;
}

}  // namespace obvr::selfcheck
