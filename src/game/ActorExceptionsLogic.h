#pragma once

// Actors left out of the fighters' ring and of the enemy attack tempo (Nadi,
// 2026-10-26: "Wir sollten beim fighters ring die Möglichkeit geben dass
// user Ausnahmen hinzufügen können. Für mich alle Kreaturen die Zombie oder
// Walker im namen haben oder sind. Selbes für Anzahl Angreifer aus dem
// Ring. Selbes für angriffstempo"). The ring and its attackers at once are
// one mechanism - whoever has no turn is held at the ring - so one list
// serves both ([Hands] CombatRingExceptions; Nadi chose it, 2026-10-26), and
// the tempo has its own ([Hands] AttackTempoExceptions).
//
// A setting is a comma-separated list; an entry matches when it stands
// anywhere in the actor's name or its race's name, any case. The Walkers'
// walkers are creatures named "Walker" by default (OblivionWalkers.esp CREA
// 0x801-0x804; [Zombies] RenameToWalker=0 names them "Zombie"), the game's
// zombies "Zombie" - the Walkers session's answer, 2026-10-26. Not by the
// plugin: that esp holds the gate guards too, who are no walkers.
//
// Pure; covered by actor_exceptions_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr int kActorExceptionsMax = 16;
inline constexpr UInt32 kActorExceptionChars = 64;

struct ActorExceptions {
	int count = 0;
	char entry[kActorExceptionsMax][kActorExceptionChars] = {};
};

// What is read about an actor; a null or empty text matches nothing.
struct ActorIdentity {
	const char* name = nullptr;    // its base's full name
	const char* race = nullptr;    // its race's name (a person's), or none
};

inline char LowerAscii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

inline ActorExceptions ParseActorExceptions(const char* setting) {
	ActorExceptions out;
	if (setting == nullptr) {
		return out;
	}
	const char* p = setting;
	while (*p != '\0' && out.count < kActorExceptionsMax) {
		while (*p == ' ' || *p == '\t' || *p == ',') {
			++p;
		}
		const char* start = p;
		while (*p != '\0' && *p != ',') {
			++p;
		}
		const char* end = p;
		while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
			--end;
		}
		if (end == start) {
			continue;
		}
		UInt32 length = static_cast<UInt32>(end - start);
		if (length > kActorExceptionChars - 1) {
			length = kActorExceptionChars - 1;
		}
		char* const e = out.entry[out.count];
		for (UInt32 i = 0; i < length; ++i) {
			e[i] = start[i];
		}
		e[length] = '\0';
		++out.count;
	}
	return out;
}

inline bool ContainsNoCase(const char* text, const char* part) {
	if (text == nullptr || part == nullptr || part[0] == '\0') {
		return false;
	}
	for (const char* t = text; *t != '\0'; ++t) {
		UInt32 i = 0;
		while (part[i] != '\0' && t[i] != '\0' && LowerAscii(t[i]) == LowerAscii(part[i])) {
			++i;
		}
		if (part[i] == '\0') {
			return true;
		}
	}
	return false;
}

inline bool ActorExcepted(const ActorExceptions& list, const ActorIdentity& who) {
	for (int i = 0; i < list.count; ++i) {
		if (ContainsNoCase(who.name, list.entry[i]) || ContainsNoCase(who.race, list.entry[i])) {
			return true;
		}
	}
	return false;
}

}  // namespace obvr::game
