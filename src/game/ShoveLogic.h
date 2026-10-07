#pragma once

// The shove: an open hand driven fast into a living actor, on purpose
// (docs/hand-weapon-collision-spec.md, "Proposal: pushing and pulling
// people on purpose"; the tester, 2026-09-29: "finde ich alles geil. können
// wir so machen"). With PushPeople off the hands pass through the living; a
// shove is the deliberate way to move someone.
//
// - Only with the weapons away (sheathed or none drawn - with a weapon or
//   the fists up a fast hand is a swing), an open hand (not a fist, the grip
//   not closed), and the hand moving towards the actor at ShoveSpeed or
//   faster: a light shove, the actor staggers. From ShoveHardSpeed a hard
//   one: knocked down, pushed away from the hand.
// - Either costs the player fatigue and lowers the actor's disposition
//   towards the player; the same actor is not shoved again for a moment.
//
// Pure, covered by shove_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

struct ShoveSettings {
	bool enabled = true;
	float speed = 1.5f;      // m/s towards the actor: a light shove (the tester's, 2026-09-29)
	float hardSpeed = 3.2f;  // m/s: a hard one, knocked down (the tester's, 2026-09-29)
	// The hard shove's force, as the engine's knockback takes it
	// (PushActorAway's: fKnockbackDamageBase 50 is a typical value).
	float hardForce = 3.0f;  // the tester, 2026-09-29: 50 threw them far too hard
	// The light shove's push: this far (game units) back, over this long - the
	// hit's own knockback through the character proxy (0x008907A0).
	float distance = 30.0f;
	float pushSeconds = 0.3f;
	float cooldownSeconds = 1.0f;
	float fatigueLight = 15.0f;
	float fatigueHard = 40.0f;  // PLANCK's shove costs 40 stamina
	float dispositionLight = 5.0f;
	float dispositionHard = 15.0f;
	// A slap: the open hand into the face rather than the body (the tester,
	// 2026-10-07: "man kann npcs ins gesicht slappen ... disposition wird
	// verloren, einstellbar") - a light shove landed kSlapFaceAboveUnits or
	// more above the body's middle costs this much instead of dispositionLight.
	float dispositionFace = 10.0f;
	// A hard shove - pushed to the ground - counts as a hit on them: the
	// engine's own reaction to being hit (the tester, 2026-09-29: "den zu boden
	// schubsen nur der zählt wie ein hit der andere schubser nicht") - a friend
	// forgives the first few, anyone else takes it as an assault. A light shove
	// only costs their liking.
	bool countsAsHit = true;
};

enum class ShoveKind : UInt8 { None, Light, Hard };

// Whether this shove goes to the engine as a hit.
inline bool ShoveCountsAsHit(const ShoveSettings& s, ShoveKind kind) {
	return s.countsAsHit && kind == ShoveKind::Hard;
}

// Whether the hand landed in the face: this far above the body's middle
// (a bound's centre is about the hips; the head is some 60 cm up).
inline constexpr float kSlapFaceAboveUnits = 25.0f;  // 35 cm
inline bool SlapInTheFace(float handZ, float bodyCentreZ) {
	return handZ - bodyCentreZ >= kSlapFaceAboveUnits;
}

// What a slap in the face sounds like (the tester, 2026-10-07: "ein schönen
// slap sound ... npc greift sich die wange wie in der mod"; then "wenn die
// mod da ist dann machen wir random den slap sound von der und unserer").
// OBVR's own slap (assets/sounds/slap.wav, CC0) always exists; with Put it
// in its Place - Enhanced Grabbing loaded its slap noise takes turns with
// it, by a coin toss. The mod's assets are not OBVR's to ship (its readme
// grants no such use), so without the mod there is only OBVR's.
enum class SlapSound : UInt8 { Own, Mod };
inline SlapSound SlapSoundFor(bool putItInItsPlaceLoaded, bool coin) {
	return putItInItsPlaceLoaded && coin ? SlapSound::Mod : SlapSound::Own;
}

// The script lines the slap runs as the slapped one: the mod's noise when
// it is the sound's turn, and with the mod its slapped idle the way its
// slapper script does it (the idle marker token, pickIdle, the token off
// again); without the mod the game's own gasp. The mod's forms by form id,
// its load index in the top byte: lines by editor id answered false and did
// nothing in the game (the tester, 2026-10-07: "die put it in its place
// script hat allerdings nicht gestartet"), form ids are what the console
// takes. `modIndex` 0 is the mod not loaded. Returns how many lines are in
// `out` (at most 4), each at most kSlapLineChars long.
inline constexpr UInt32 kPiiiPSlapNoiseLow = 0x005339;          // SOUN zzPiiiPSlapNoise
inline constexpr UInt32 kPiiiPSlappedTokenLow = 0x005335;       // CLOT zzPiiiPIdleMarkerSlappedToken
inline constexpr UInt32 kSlapLineChars = 48;

inline void SlapFormLine(char* out, const char* command, UInt8 modIndex, UInt32 low, const char* tail) {
	static const char kHex[] = "0123456789ABCDEF";
	UInt32 n = 0;
	for (const char* c = command; *c != '\0' && n + 12 < kSlapLineChars; ++c) {
		out[n++] = *c;
	}
	out[n++] = ' ';
	const UInt32 id = (static_cast<UInt32>(modIndex) << 24) | (low & 0xFFFFFFu);
	for (int shift = 28; shift >= 0; shift -= 4) {
		out[n++] = kHex[(id >> shift) & 0xF];
	}
	for (const char* c = tail; *c != '\0' && n + 1 < kSlapLineChars; ++c) {
		out[n++] = *c;
	}
	out[n] = '\0';
}

inline UInt32 SlapLines(UInt8 modIndex, SlapSound sound, char out[4][kSlapLineChars]) {
	UInt32 n = 0;
	if (modIndex != 0) {
		if (sound == SlapSound::Mod) {
			SlapFormLine(out[n++], "playSound3D", modIndex, kPiiiPSlapNoiseLow, "");
		}
		SlapFormLine(out[n++], "addItemNS", modIndex, kPiiiPSlappedTokenLow, " 1");
		for (const char* c = "pickIdle"; ; ++c) {
			out[n][c - "pickIdle"] = *c;
			if (*c == '\0') {
				break;
			}
		}
		++n;
		SlapFormLine(out[n++], "removeItemNS", modIndex, kPiiiPSlappedTokenLow, " 1");
		return n;
	}
	const char* const gasp = "playSound3D NPCHumanGaspMale";
	UInt32 i = 0;
	for (; gasp[i] != '\0'; ++i) {
		out[n][i] = gasp[i];
	}
	out[n][i] = '\0';
	return n + 1;
}

// How the mod's own slap is set off when it is loaded: its grab script
// slaps when the grab key is tapped with an actor under the pick (its
// "Tap-Slap"; the lines by form id above answered false and did nothing in
// the tester's log, 2026-10-07). So a slap in the face with the mod loaded
// taps the grab for kSlapGrabTapFrames with the pick on the slapped one,
// and the mod does the rest - its noise, the cheek, the reactions; OBVR's
// own noise stays out of its way then. Without the mod: OBVR's noise and
// the gasp line.
//
// What the mod's grab script does with the tap (its zzPiiiPGrabQS, read
// from the ESP 2026-10-07): on the grab control's down it takes the
// crosshair's reference; a living NPC starts a count of the frames the
// control stays down - 12 or more is its pickpocket, let go within 7 is its
// Tap-Slap. The slap then wants neither side in combat, the player facing
// the NPC within 25 degrees and the NPC facing the player within 18 ("I
// can't slap %po from this side"), the NPC not seated, a height difference
// within 0.1 of scale and 12 units, the actors' origins within 51 units
// ("Damnation ..., %ps's out of reach"), and the crosshair's spot on the
// head. Then both are restrained, the NPC is set 50 units before the player,
// both play their idles (the slap, the cheek), and its reaction handler
// takes half the NPC's disposition, has them curse, slap back or challenge
// to a duel. The tap is pressed only after kSlapGrabAimFrames with the pick
// already on them, so the down-edge finds the crosshair's reference there.
// A slap the mod finishes is wholly its own: no stagger, push, fatigue or
// disposition from OBVR - its positioner needs them where they stand, and
// its handler costs them their liking.
inline constexpr UInt32 kSlapGrabTapFrames = 3;
inline constexpr UInt32 kSlapGrabAimFrames = 3;
inline constexpr UInt32 kSlapGrabTapTotalFrames = kSlapGrabTapFrames + kSlapGrabAimFrames;
inline bool SlapByModsGrabTap(UInt8 modIndex) { return modIndex != 0; }
// Whether the grab is down this frame of the tap, `framesLeft` counting
// down from kSlapGrabTapTotalFrames: the aim frames first, then the press.
inline bool SlapGrabTapPressed(UInt32 framesLeft) { return framesLeft > 0 && framesLeft <= kSlapGrabTapFrames; }
// Whether a shove is a slap the mod finishes - a light one in the face with
// the mod loaded - and so none of OBVR's own.
inline bool SlapLeftToMod(UInt8 modIndex, ShoveKind kind, bool inTheFace) {
	return SlapByModsGrabTap(modIndex) && kind == ShoveKind::Light && inTheFace;
}
inline constexpr const char* kSlapWave = "OBVR_Sounds\\slap.wav";

// What a shove costs the actor's liking: the hard shove's, or for a light
// one the face's or the body's.
inline float ShoveDisposition(const ShoveSettings& s, ShoveKind kind, bool inTheFace) {
	if (kind == ShoveKind::Hard) {
		return s.dispositionHard;
	}
	return inTheFace ? s.dispositionFace : s.dispositionLight;
}

// What the hand is doing now.
struct ShoveHand {
	bool valid = false;
	bool open = false;         // not a fist
	bool gripHeld = false;
	float towardsSpeed = 0.0f; // m/s along the line to the actor
};

inline ShoveKind ShoveFor(const ShoveSettings& s, bool weaponDrawn, const ShoveHand& hand) {
	if (!s.enabled || weaponDrawn || !hand.valid || !hand.open || hand.gripHeld) {
		return ShoveKind::None;
	}
	if (!(hand.towardsSpeed >= s.speed)) {
		return ShoveKind::None;
	}
	return hand.towardsSpeed >= s.hardSpeed ? ShoveKind::Hard : ShoveKind::Light;
}

// The hand's speed towards a point, across the ground only (a hand pushed
// down onto someone's head is no shove): the velocity's part along the
// horizontal line from the hand to the point. Negative moving away.
inline float SpeedTowards(const NiPoint3& velocity, const NiPoint3& hand, const NiPoint3& target) {
	const float dx = target.x - hand.x;
	const float dy = target.y - hand.y;
	const float length = math::Sqrt(dx * dx + dy * dy);
	if (!(length > 0.0001f)) {
		return 0.0f;
	}
	return (velocity.x * dx + velocity.y * dy) / length;
}

// Whether a point lies at an actor's body, taken as an upright column: within
// `sideFactor` of its bound's radius plus `padUnits` across the ground, and
// within `heightFactor` of the radius plus `padUnits` above or below the
// bound's centre. The bound's sphere is as wide as the actor is tall, so a
// ball of half its radius round the centre reached the chest and never the
// head (the tester, 2026-09-29: "ich kann sie nur an der brust schubsen nicht
// am kopf").
inline bool HandAtBody(const NiPoint3& hand, const NiPoint3& centre, float radius, float sideFactor,
                       float heightFactor, float padUnits) {
	const float dx = hand.x - centre.x;
	const float dy = hand.y - centre.y;
	const float dz = hand.z - centre.z;
	const float side = radius * sideFactor + padUnits;
	const float height = radius * heightFactor + padUnits;
	return dx * dx + dy * dy <= side * side && (dz < 0.0f ? -dz : dz) <= height;
}

// Whether a point lies in an actor's reach: within its bound's radius times
// `factor` plus `padUnits` of the bound's centre.
inline bool HandAtActor(const NiPoint3& hand, const NiPoint3& centre, float radius, float factor, float padUnits) {
	const float reach = radius * factor + padUnits;
	return (hand - centre).LengthSquared() <= reach * reach;
}

// The light shove's push: `distance` units along the ground, away from the
// hand (from `from` towards the actor's centre). Nothing when the two stand
// in one place.
inline NiPoint3 ShovePush(const NiPoint3& from, const NiPoint3& centre, float distance) {
	const float dx = centre.x - from.x;
	const float dy = centre.y - from.y;
	const float length = math::Sqrt(dx * dx + dy * dy);
	if (!(length > 0.0001f) || !(distance > 0.0f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return NiPoint3{dx / length * distance, dy / length * distance, 0.0f};
}

// One actor shoved not again until the cooldown has run out.
struct ShoveCooldown {
	const void* actor = nullptr;
	float secondsLeft = 0.0f;
};

inline void StepShoveCooldown(ShoveCooldown& c, float dt) {
	if (c.secondsLeft > 0.0f) {
		c.secondsLeft -= dt;
		if (c.secondsLeft <= 0.0f) {
			c.secondsLeft = 0.0f;
			c.actor = nullptr;
		}
	}
}

inline bool ShoveAllowed(const ShoveCooldown& c, const void* actor) {
	return actor != nullptr && !(c.actor == actor && c.secondsLeft > 0.0f);
}

inline void StartShoveCooldown(ShoveCooldown& c, const void* actor, float seconds) {
	c.actor = actor;
	c.secondsLeft = seconds;
}

}  // namespace obvr::game
