#include "game/ActorExceptions.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/ActorExceptionsLogic.h"
#include "game/GameAddresses.h"
#include "game/QuickKeys.h"

namespace obvr::game {
namespace {

// A person's race: TESNPC +0xE8 holds the TESRace (TESRaceForm at +0xE4;
// xOBSE GameForms.h, read again by the Walkers session for its race swap).
constexpr UInt32 kNpcRaceOffset = 0xE8;
constexpr UInt32 kRememberedMax = 32;

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }

struct Remembered {
	UInt32 actor = 0;
	UInt32 base = 0;
	bool excepted = false;
};

struct List {
	char setting[256] = "";
	ActorExceptions parsed;
	Remembered remembered[kRememberedMax];
	UInt32 next = 0;  // the slot taken next when the table is full
	UInt32 lines = 20;
	const char* name = "";
};

List g_lists[2];

List& ListOf(ExceptionList which) { return g_lists[which == ExceptionList::CombatRing ? 0 : 1]; }

bool SameText(const char* a, const char* b) {
	UInt32 i = 0;
	for (; a[i] != '\0' && b[i] != '\0'; ++i) {
		if (a[i] != b[i]) {
			return false;
		}
	}
	return a[i] == b[i];
}

}  // namespace

void SetActorExceptions(ExceptionList which, const char* setting) {
	List& list = ListOf(which);
	list.name = which == ExceptionList::CombatRing ? "CombatRingExceptions" : "AttackTempoExceptions";
	const char* text = setting != nullptr ? setting : "";
	if (SameText(list.setting, text)) {
		return;
	}
	UInt32 i = 0;
	for (; i + 1 < sizeof(list.setting) && text[i] != '\0'; ++i) {
		list.setting[i] = text[i];
	}
	list.setting[i] = '\0';
	list.parsed = ParseActorExceptions(list.setting);
	for (Remembered& r : list.remembered) {
		r = Remembered{};
	}
	list.lines = 20;
	OBVR_LOG("Exceptions: [Hands] %s = \"%s\" - %d entr%s", list.name, list.setting, list.parsed.count,
	         list.parsed.count == 1 ? "y" : "ies");
}

bool ActorOnExceptionList(ExceptionList which, UInt32 actor) {
	List& list = ListOf(which);
	if (list.parsed.count == 0 || !mem::LooksLikeObjectAddress(actor)) {
		return false;
	}
	const UInt32 base = Read(actor + addr::kRefBaseFormOffset);
	for (const Remembered& r : list.remembered) {
		if (r.actor == actor && r.base == base) {
			return r.excepted;
		}
	}
	char name[96];
	char race[96];
	ReadFormFullName(base, name, sizeof(name));
	race[0] = '\0';
	if (Read(actor) == addr::kVtblCharacter && mem::LooksLikeObjectAddress(base)) {
		ReadFormFullName(Read(base + kNpcRaceOffset), race, sizeof(race));
	}
	ActorIdentity who;
	who.name = name;
	who.race = race;
	const bool excepted = ActorExcepted(list.parsed, who);
	Remembered* slot = nullptr;
	for (Remembered& r : list.remembered) {
		if (r.actor == 0) {
			slot = &r;
			break;
		}
	}
	if (slot == nullptr) {
		slot = &list.remembered[list.next];
		list.next = (list.next + 1) % kRememberedMax;
	}
	slot->actor = actor;
	slot->base = base;
	slot->excepted = excepted;
	if (excepted && list.lines > 0) {
		--list.lines;
		OBVR_LOG("Exceptions: %08X (\"%s\"%s%s%s) is on [Hands] %s - left to the game", actor, name,
		         race[0] != '\0' ? ", race \"" : "", race, race[0] != '\0' ? "\"" : "", list.name);
	}
	return excepted;
}

}  // namespace obvr::game
