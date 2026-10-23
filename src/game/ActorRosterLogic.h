#pragma once

// The loaded actors at a hand script's mark: the "HandScript: actors" line
// ([Debug] HandScript, docs/hand-script-harness.md). A scenario that places,
// spawns, kills or raises people reads off this line how many of each base
// form stand in the world, alive and dead, and its console-at lines get the
// nearest one as {actor} (test/HandScriptConsole.h). Built 2026-10-23 for
// the Oblivion Walkers scenarios (a leveled list's count, a corpse raised by
// the console) - until then no mark line counted actors.
//
// Pure: the grouping and the line's text over plain values; the engine's
// walk of the process lists is game/ActorRoster.h. Covered by
// actor_roster_test.

#include <cstdio>
#include <cstring>

#include "core/Types.h"

namespace obvr::game {

// One loaded actor as the roster reads it.
struct RosterActor {
	UInt32 formId = 0;      // the reference
	UInt32 baseFormId = 0;  // its base (an NPC_ or CREA record)
	bool dead = false;
	float distance = 0.0f;  // units from the player
};

// The bases named in the line, in first-seen order; the rest are counted
// as "and N more".
inline constexpr UInt32 kRosterGroupsMost = 24;

// The index of the actor nearest the player, -1 with none.
inline int NearestRosterActor(const RosterActor* actors, UInt32 count) {
	int nearest = -1;
	for (UInt32 i = 0; i < count; ++i) {
		if (nearest < 0 || actors[i].distance < actors[nearest].distance) {
			nearest = static_cast<int>(i);
		}
	}
	return nearest;
}

// The line's text after "HandScript: actors - ":
//   "3 loaded, 1 dead: 00069AD1 x2 (dead 1), 00025E6E x1; nearest FF000A3B at 120 units (dead)"
// Bases in first-seen order, each with how many of it stand there and, when
// any, how many of those are dead; "and N more" past kRosterGroupsMost
// bases; "none" with nobody loaded. Always NUL-terminated, cut to `size`.
inline void FormatRosterLine(const RosterActor* actors, UInt32 count, char* buf, size_t size) {
	if (size == 0) {
		return;
	}
	buf[0] = '\0';
	UInt32 bases[kRosterGroupsMost];
	UInt32 alive[kRosterGroupsMost] = {};
	UInt32 dead[kRosterGroupsMost] = {};
	UInt32 groups = 0;
	UInt32 more = 0;
	UInt32 deadTotal = 0;
	for (UInt32 i = 0; i < count; ++i) {
		if (actors[i].dead) {
			++deadTotal;
		}
		UInt32 g = 0;
		while (g < groups && bases[g] != actors[i].baseFormId) {
			++g;
		}
		if (g == groups) {
			if (groups == kRosterGroupsMost) {
				++more;
				continue;
			}
			bases[groups++] = actors[i].baseFormId;
		}
		if (actors[i].dead) {
			++dead[g];
		} else {
			++alive[g];
		}
	}
	size_t at = static_cast<size_t>(std::snprintf(buf, size, "%u loaded, %u dead: ", count, deadTotal));
	if (at >= size) {
		return;
	}
	if (count == 0) {
		std::snprintf(buf + at, size - at, "none");
		return;
	}
	for (UInt32 g = 0; g < groups && at < size; ++g) {
		int n = 0;
		if (dead[g] > 0) {
			n = std::snprintf(buf + at, size - at, "%s%08X x%u (dead %u)", g > 0 ? ", " : "", bases[g],
			                  alive[g] + dead[g], dead[g]);
		} else {
			n = std::snprintf(buf + at, size - at, "%s%08X x%u", g > 0 ? ", " : "", bases[g], alive[g]);
		}
		at += n > 0 ? static_cast<size_t>(n) : 0;
	}
	if (more > 0 && at < size) {
		const int n = std::snprintf(buf + at, size - at, " and %u more", more);
		at += n > 0 ? static_cast<size_t>(n) : 0;
	}
	const int nearest = NearestRosterActor(actors, count);
	if (nearest >= 0 && at < size) {
		std::snprintf(buf + at, size - at, "; nearest %08X at %.0f units%s", actors[nearest].formId,
		              static_cast<double>(actors[nearest].distance), actors[nearest].dead ? " (dead)" : "");
	}
	buf[size - 1] = '\0';
}

}  // namespace obvr::game
