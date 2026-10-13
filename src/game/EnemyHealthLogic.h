#pragma once

// The enemy health bars over the fighters (docs/hud-on-hands-spec.md; the
// tester, 2026-09-29: "wir müssen den healthbar von gegnern rendern ... das
// ist afaik ein vanilla feature", and 2026-10-13: "Gegner-Lebensbalken über
// dem NPC in der Welt. nicht mehr am crosshair. default an").
//
// Vanilla draws one bar, HUDReticle's enemy health, at the crosshair for
// the fighter the crosshair is on. In the headset the crosshair hangs on the
// laser or over the thing pointed at, and a fight is not fought by pointing:
// so every fighter whose combat target is the player gets a bar of their own
// in the world, above their head, turned to the eyes - the engine's current
// and base health as a fraction. The bar stays a moment after they die (at
// nothing) or stop fighting, then goes. The game's own bar is hidden while
// this is on (game/WorldPickHook, after HUDReticle's update).
//
// Pure, covered by enemy_health_test.

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

struct EnemyHealthSettings {
	bool enabled = true;         // [HandHud] EnemyHealthBars
	float rangeMetres = 25.0f;   // [HandHud] EnemyHealthMetres: farther than this, no bar
	float widthMetres = 0.35f;   // [HandHud] EnemyHealthWidthMetres: the bar's width in the world, near
};

// How many bars at once; the nearest fighters get them.
inline constexpr UInt32 kEnemyHealthBarsMax = 8;
// A bar stays this long after its fighter dies or stops fighting.
inline constexpr float kEnemyHealthLingerSeconds = 2.0f;
// Above the head bone, game units (about 20 cm): clear of the hair and a
// helmet. Without the head bone: the feet plus a person's height, times the
// skeleton's scale.
inline constexpr float kEnemyHealthAboveHeadUnits = 14.0f;
inline constexpr float kEnemyHealthAboveFeetUnits = 136.0f;
// The bar never narrower than this share of its distance (about 3.4
// degrees), so a far bar still reads.
inline constexpr float kEnemyHealthMinWidthPerMetre = 0.06f;
// A repaint when the fraction moved this much.
inline constexpr float kEnemyHealthRepaintStep = 1.0f / 128.0f;

// The settings as used: the range 1 to 100 m, else 25; the width 0.1 to 2 m,
// else 0.35.
inline float EnemyHealthRangeMetres(float setting) {
	return setting >= 1.0f && setting <= 100.0f ? setting : 25.0f;
}
inline float EnemyHealthWidthMetres(float setting) {
	return setting >= 0.1f && setting <= 2.0f ? setting : 0.35f;
}

// The bar's width in the world at this distance: the set width, or wider so
// it keeps a minimum apparent size.
inline float EnemyHealthBarWidth(float widthMetres, float distanceMetres) {
	const float floor = distanceMetres * kEnemyHealthMinWidthPerMetre;
	return floor > widthMetres ? floor : widthMetres;
}

// Health as a share of the base, 0 to 1. No base (0 or less, or not a
// number): full while anything is left, else empty.
inline float EnemyHealthFraction(float now, float base) {
	if (!(now == now) || !(now > 0.0f)) {
		return 0.0f;
	}
	if (!(base > 0.0f)) {
		return 1.0f;
	}
	const float f = now / base;
	return f > 1.0f ? 1.0f : f;
}

// Where the bar hangs: above the head bone when it was read, else above
// the feet by a person's height, scaled.
inline NiPoint3 EnemyHealthBarPoint(bool haveHead, const NiPoint3& head, const NiPoint3& feet, float scale) {
	const float k = scale > 0.0f && scale < 20.0f ? scale : 1.0f;
	if (haveHead) {
		return NiPoint3{head.x, head.y, head.z + kEnemyHealthAboveHeadUnits * k};
	}
	return NiPoint3{feet.x, feet.y, feet.z + kEnemyHealthAboveFeetUnits * k};
}

// Whether a new fraction wants the bar painted anew.
inline bool EnemyHealthRepaintDue(float painted, float now) {
	const float d = painted > now ? painted - now : now - painted;
	return d >= kEnemyHealthRepaintStep || (now <= 0.0f) != (painted <= 0.0f);
}

// One fighter read this frame (game/EnemyHealth.h): alive and fighting the
// player, or - one that had a bar - dead or no longer fighting.
struct EnemyHealthSeen {
	UInt32 actor = 0;
	bool fighting = false;  // alive, in combat, the player their target
	bool dead = false;
	bool haveHead = false;  // the bar's point is above the head bone (else above the feet)
	float fraction = 0.0f;
	float metres = 0.0f;              // from the eyes
	NiPoint3 at{0.0f, 0.0f, 0.0f};  // the bar's point, world
};

struct EnemyHealthBar {
	UInt32 actor = 0;  // 0: free
	float fraction = 0.0f;
	float painted = -1.0f;   // the fraction last painted; -1 never
	UInt32 revision = 0;     // for the canvas: changed when painted anew
	float metres = 0.0f;
	NiPoint3 at{0.0f, 0.0f, 0.0f};
	float linger = 0.0f;   // seconds not fighting (or dead)
	float seconds = 0.0f;  // how long the bar has shown
	bool dead = false;
};

struct EnemyHealthState {
	EnemyHealthBar bar[kEnemyHealthBarsMax];
};

inline SInt32 EnemyHealthBarOf(const EnemyHealthState& s, UInt32 actor) {
	if (actor == 0) {
		return -1;
	}
	for (UInt32 i = 0; i < kEnemyHealthBarsMax; ++i) {
		if (s.bar[i].actor == actor) {
			return static_cast<SInt32>(i);
		}
	}
	return -1;
}

struct EnemyHealthVerdict {
	UInt32 shown = 0;    // bars with a fighter this frame
	UInt32 started = 0;  // bars that began this frame
	UInt32 ended = 0;    // bars that went this frame
};

// Once a frame: the bars kept for the fighters seen, begun for new ones
// (the nearest first while slots are free), kept lingering for those no
// longer fighting or dead - their last point, the fraction 0 when dead - and
// let go after the linger. Farther than `rangeMetres` is not seen (the
// caller leaves them out). A bar's revision steps when its fraction moved
// enough to paint anew.
inline EnemyHealthVerdict StepEnemyHealthBars(EnemyHealthState& s, const EnemyHealthSeen* seen, UInt32 seenCount,
                                              float dtSeconds) {
	EnemyHealthVerdict v;
	const float dt = dtSeconds > 0.0f ? dtSeconds : 0.0f;
	bool used[kEnemyHealthBarsMax] = {};
	// The bars that have a fighter: refreshed, or set lingering.
	for (UInt32 i = 0; i < kEnemyHealthBarsMax; ++i) {
		EnemyHealthBar& b = s.bar[i];
		if (b.actor == 0) {
			continue;
		}
		const EnemyHealthSeen* found = nullptr;
		for (UInt32 k = 0; k < seenCount; ++k) {
			if (seen[k].actor == b.actor) {
				found = &seen[k];
				break;
			}
		}
		b.seconds += dt;
		if (found != nullptr) {
			b.at = found->at;
			b.metres = found->metres;
			b.dead = found->dead;
			b.fraction = found->dead ? 0.0f : found->fraction;
		}
		if (found != nullptr && found->fighting && !found->dead) {
			b.linger = 0.0f;
		} else {
			b.linger += dt;
			if (b.linger >= kEnemyHealthLingerSeconds) {
				b = EnemyHealthBar{};
				++v.ended;
				continue;
			}
		}
		used[i] = true;
		if (b.painted < 0.0f || EnemyHealthRepaintDue(b.painted, b.fraction)) {
			b.painted = b.fraction;
			++b.revision;
		}
		++v.shown;
	}
	// New fighters, the nearest first, into the free slots.
	for (;;) {
		SInt32 best = -1;
		for (UInt32 k = 0; k < seenCount; ++k) {
			if (!seen[k].fighting || seen[k].dead || seen[k].actor == 0 || EnemyHealthBarOf(s, seen[k].actor) >= 0) {
				continue;
			}
			if (best < 0 || seen[k].metres < seen[best].metres) {
				best = static_cast<SInt32>(k);
			}
		}
		if (best < 0) {
			break;
		}
		SInt32 slot = -1;
		for (UInt32 i = 0; i < kEnemyHealthBarsMax; ++i) {
			if (!used[i] && s.bar[i].actor == 0) {
				slot = static_cast<SInt32>(i);
				break;
			}
		}
		if (slot < 0) {
			break;
		}
		EnemyHealthBar& b = s.bar[slot];
		b = EnemyHealthBar{};
		b.actor = seen[best].actor;
		b.fraction = seen[best].fraction;
		b.at = seen[best].at;
		b.metres = seen[best].metres;
		b.painted = b.fraction;
		b.revision = 1;
		used[slot] = true;
		++v.started;
		++v.shown;
	}
	return v;
}

}  // namespace obvr::game
