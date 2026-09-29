#pragma once

// Taking someone by the hand (docs/hand-weapon-collision-spec.md, the shove
// proposal's step 2; the tester, 2026-09-29: "npcs an die hand nehmen und
// sie folgen einem wie in skyrimVR ... npcs die keine follower sind folgen
// nur bis zu 5m und verlieren disposition. follower folgen solange wie man
// sie hält").
//
// A grip closed next to a living person's hand takes it. While it is held
// they walk after the player. A follower walks along for as long as the grip
// stays closed; anyone else only for so far, their liking for the player
// dropping as they go, and then pulls free.
//
// Pure, covered by lead_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

struct LeadSettings {
	bool enabled = true;
	// The grip takes a hand within this many game units of it (70 a metre).
	float takeUnits = 14.0f;
	// Someone who is not a follower pulls free after this many metres.
	float strangerMetres = 5.0f;
	// And likes the player this much less per metre led.
	float dispositionPerMetre = 3.0f;
	// The held hand further than this from the player's (game units) - a
	// wall between, or they could not keep up: let go.
	float breakUnits = 300.0f;  // they walk about 2.5 m behind (the harness, 2026-09-29)
	// How close the follow package keeps them, game units (its target's distance).
	UInt32 followUnits = 40;
};

// Whether a closed grip takes this hand: the grip has just closed, empty,
// near a living person's hand, not in combat.
inline bool LeadTakes(const LeadSettings& s, bool gripClosedNow, bool handEmpty, float distanceToTheirHandUnits,
                      bool theyAreInCombat) {
	return s.enabled && gripClosedNow && handEmpty && !theyAreInCombat &&
	       distanceToTheirHandUnits == distanceToTheirHandUnits && distanceToTheirHandUnits <= s.takeUnits;
}

enum class LeadEnd : UInt8 {
	None,        // still held
	LetGo,       // the grip opened
	PulledFree,  // a stranger has been led far enough
	TooFar,      // the hands came apart
	Gone,        // they died, fell or went away
};

struct LeadState {
	bool active = false;
	bool follower = false;
	float metres = 0.0f;           // led so far
	float dispositionOwed = 0.0f;  // not yet taken off, below one point
};

inline void StartLead(LeadState& s, bool follower) {
	s = LeadState{};
	s.active = true;
	s.follower = follower;
}

// One frame of leading: `movedMetres` how far they walked since the last
// frame. Answers why it ends, or None; `dispositionOut` the whole points to
// take off their liking this frame (strangers only).
inline LeadEnd StepLead(LeadState& s, const LeadSettings& settings, bool gripStillClosed, bool theyAreFine,
                        float handsApartUnits, float movedMetres, SInt32& dispositionOut) {
	dispositionOut = 0;
	if (!s.active) {
		return LeadEnd::None;
	}
	LeadEnd end = LeadEnd::None;
	if (!theyAreFine) {
		end = LeadEnd::Gone;
	} else if (!gripStillClosed) {
		end = LeadEnd::LetGo;
	} else if (!(handsApartUnits <= settings.breakUnits)) {
		end = LeadEnd::TooFar;
	}
	if (end == LeadEnd::None && movedMetres > 0.0f && movedMetres == movedMetres) {
		s.metres += movedMetres;
		if (!s.follower) {
			s.dispositionOwed += movedMetres * settings.dispositionPerMetre;
			const SInt32 whole = static_cast<SInt32>(s.dispositionOwed);
			if (whole > 0) {
				dispositionOut = whole;
				s.dispositionOwed -= static_cast<float>(whole);
			}
			if (s.metres >= settings.strangerMetres) {
				end = LeadEnd::PulledFree;
			}
		}
	}
	if (end != LeadEnd::None) {
		s.active = false;
	}
	return end;
}

inline const char* LeadEndName(LeadEnd e) {
	switch (e) {
	case LeadEnd::LetGo:
		return "let go";
	case LeadEnd::PulledFree:
		return "pulled free - they will not be led further";
	case LeadEnd::TooFar:
		return "the hands came apart";
	case LeadEnd::Gone:
		return "they can no longer be led";
	default:
		return "held";
	}
}

}  // namespace obvr::game
