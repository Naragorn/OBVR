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
// Since 2026-10-25 a bar shows only when it tells something (the tester:
// "nur wenn in line of sight mit player headset ... und nur wenn <100% ...
// und soll sich auch ausblenden iwann", and "ratten die nur am boden sind
// haben 2m über sich den healthbar"): while the fighter is hurt, for a few
// seconds after their health last moved, then faded out; only while nothing
// solid lies between the eyes and them; and over their own top - the head
// bone, else the highest of their bones, else their model's bound - never a
// human's height over a rat's feet.
//
// Pure, covered by enemy_health_test.

#include <cstdio>
#include <cstring>

#include "core/AddressSpace.h"
#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

struct EnemyHealthSettings {
	bool enabled = true;         // [HandHud] EnemyHealthBars
	float rangeMetres = 25.0f;   // [HandHud] EnemyHealthMetres: farther than this, no bar
	float widthMetres = 0.35f;   // [HandHud] EnemyHealthWidthMetres: the bar's width in the world, near
	float aboveMetres = 0.25f;   // [HandHud] EnemyHealthAboveMetres: the bar's centre this far above the head bone
	float showSeconds = 4.0f;    // [HandHud] EnemyHealthShowSeconds: how long a bar stays after the health moved
};

// How many bars at once; the nearest fighters get them.
inline constexpr UInt32 kEnemyHealthBarsMax = 8;
// A bar stays this long after its fighter dies or stops fighting.
inline constexpr float kEnemyHealthLingerSeconds = 2.0f;
// The head bone's height over the feet, game units, for an actor whose
// skeleton cannot be read: the bar hangs above that as it hangs above the
// bone.
inline constexpr float kEnemyHealthHeadAboveFeetUnits = 122.0f;
// The bar never narrower than this share of its distance (about 3.4
// degrees), so a far bar still reads.
inline constexpr float kEnemyHealthMinWidthPerMetre = 0.06f;
// A repaint when the fraction moved this much.
inline constexpr float kEnemyHealthRepaintStep = 1.0f / 128.0f;

// The settings as used: the range 1 to 100 m, else 25; the width 0.1 to 2 m,
// else 0.35; the raise above the head bone 0.05 to 1 m, else 0.25 (the
// tester, 2026-10-13: the bars 0.2 m above the bone "müssen bischen höher";
// 2026-10-14, at 0.32 m: "die npc healtbars müssen noch etwas runter").
inline float EnemyHealthRangeMetres(float setting) {
	return setting >= 1.0f && setting <= 100.0f ? setting : 25.0f;
}
inline float EnemyHealthWidthMetres(float setting) {
	return setting >= 0.1f && setting <= 2.0f ? setting : 0.35f;
}
inline float EnemyHealthAboveMetres(float setting) {
	return setting >= 0.05f && setting <= 1.0f ? setting : 0.25f;
}
// How long a bar stays after the health last moved: 1 to 60 s, else 4.
inline float EnemyHealthShowSeconds(float setting) {
	return setting >= 1.0f && setting <= 60.0f ? setting : 4.0f;
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

// Where the bar hangs: `aboveUnits` (the raise in game units) above the head
// bone when it was read, else above the feet by the head's height and the
// same raise; both scaled by the skeleton's scale. A raise that is not a
// number, negative or absurd puts the bar on the bone.
inline NiPoint3 EnemyHealthBarPoint(bool haveHead, const NiPoint3& head, const NiPoint3& feet, float scale,
                                    float aboveUnits) {
	const float k = scale > 0.0f && scale < 20.0f ? scale : 1.0f;
	const float up = aboveUnits >= 0.0f && aboveUnits < 1000.0f ? aboveUnits : 0.0f;
	if (haveHead) {
		return NiPoint3{head.x, head.y, head.z + up * k};
	}
	return NiPoint3{feet.x, feet.y, feet.z + (kEnemyHealthHeadAboveFeetUnits + up) * k};
}

// ------------------------------------------------- the fighter's own top
// What a bar hangs over (the tester, 2026-10-25: "ratten die nur am boden
// sind haben 2m über sich den healthbar"). Read off the vanilla skeletons
// (pyffi, 2026-10-25): most creatures carry "Bip01 Head" as people do (a
// boar at 42 units, a dog at 57, a bear at 129), but the rat, the mudcrab,
// the clannfear, the daedroth and the horse do not - the rat's highest bone
// is its spine at 28 units, the mudcrab has none of the trunk's bones, only
// its claws', and the horse is all "Bip02". Their bars went over the feet
// by a human head's 122 units.
enum class EnemyHealthTop : UInt8 {
	HeadBone,     // "Bip01 Head": the raise above it, as before
	HighestBone,  // no head bone: the highest of the body's bones read now
	Bound,        // no bone read: the top of the model's bound
	Feet,         // no model: the head's height over the feet - nothing drawn there to see
};

inline const char* EnemyHealthTopName(EnemyHealthTop top) {
	switch (top) {
		case EnemyHealthTop::HeadBone:
			return "the head bone";
		case EnemyHealthTop::HighestBone:
			return "its highest bone (no head bone)";
		case EnemyHealthTop::Bound:
			return "its model's bound (no bone read)";
		default:
			return "the head's height over the feet (no model read)";
	}
}

// The bones the reader hands over besides the head (BladeBone's others).
inline constexpr UInt32 kEnemyHealthBodyBonesMax = 24;
// Which part of the body a bone is: the trunk's are taken first, then where
// the limbs join it, then the limbs themselves - so a raised claw or paw
// does not lift the bar (derived: a mudcrab has no trunk bones; in the
// harness of 2026-10-25 its bar stood 0.64-0.73 m over its feet with every
// bone alike and 0.57-0.64 m with the tiers).
enum class EnemyHealthBoneTier : UInt8 {
	Trunk,     // neck, spine, pelvis
	LimbRoot,  // upper arms, thighs
	Limb,      // forearms, hands, calves, feet
};
// A bound larger than this is no fighter's.
inline constexpr float kEnemyHealthBoundMaxUnits = 4096.0f;

// What was read of a fighter for the bar's height, world.
struct EnemyHealthBody {
	bool haveHead = false;
	NiPoint3 head{0.0f, 0.0f, 0.0f};
	UInt32 boneCount = 0;  // the other bones read now
	NiPoint3 bone[kEnemyHealthBodyBonesMax] = {};
	EnemyHealthBoneTier tier[kEnemyHealthBodyBonesMax] = {};
	bool haveBound = false;  // the model's bound, read
	NiPoint3 boundCentre{0.0f, 0.0f, 0.0f};
	float boundRadius = 0.0f;
	NiPoint3 feet{0.0f, 0.0f, 0.0f};
	float scale = 1.0f;  // the skeleton's
};

// Where a bar hangs and what the eyes look at to see its fighter.
struct EnemyHealthAnchor {
	EnemyHealthTop from = EnemyHealthTop::Feet;
	NiPoint3 bar{0.0f, 0.0f, 0.0f};   // the bar's centre
	NiPoint3 look{0.0f, 0.0f, 0.0f};  // the eyes' ray goes here: the bone, or the bound's centre
};

// The bar's place over the fighter's own top, `aboveUnits` up (scaled by the
// skeleton as EnemyHealthBarPoint does): the head bone; else the highest of
// the other bones read, of the first tier that has one (a crawling
// creature's bar low, a rat's over its spine, a mudcrab's over the roots of
// its claws, not their raised tips); else the top of the model's bound over
// its centre; else, with no model at all, the head's height over the feet.
inline EnemyHealthAnchor EnemyHealthAnchorOf(const EnemyHealthBody& body, float aboveUnits) {
	EnemyHealthAnchor a;
	const float k = body.scale > 0.0f && body.scale < 20.0f ? body.scale : 1.0f;
	const float up = aboveUnits >= 0.0f && aboveUnits < 1000.0f ? aboveUnits : 0.0f;
	if (body.haveHead) {
		a.from = EnemyHealthTop::HeadBone;
		a.look = body.head;
		a.bar = EnemyHealthBarPoint(true, body.head, body.feet, body.scale, aboveUnits);
		return a;
	}
	const UInt32 bones = body.boneCount < kEnemyHealthBodyBonesMax ? body.boneCount : kEnemyHealthBodyBonesMax;
	SInt32 highest = -1;
	for (UInt8 tier = 0; tier <= static_cast<UInt8>(EnemyHealthBoneTier::Limb) && highest < 0; ++tier) {
		for (UInt32 i = 0; i < bones; ++i) {
			const float z = body.bone[i].z;
			if (static_cast<UInt8>(body.tier[i]) == tier && z == z && (highest < 0 || z > body.bone[highest].z)) {
				highest = static_cast<SInt32>(i);
			}
		}
	}
	if (highest >= 0) {
		a.from = EnemyHealthTop::HighestBone;
		a.look = body.bone[highest];
		a.bar = NiPoint3{a.look.x, a.look.y, a.look.z + up * k};
		return a;
	}
	if (body.haveBound && body.boundRadius > 0.0f && body.boundRadius < kEnemyHealthBoundMaxUnits) {
		a.from = EnemyHealthTop::Bound;
		a.look = body.boundCentre;
		a.bar = NiPoint3{body.boundCentre.x, body.boundCentre.y, body.boundCentre.z + body.boundRadius + up * k};
		return a;
	}
	a.from = EnemyHealthTop::Feet;
	a.bar = EnemyHealthBarPoint(false, body.head, body.feet, body.scale, aboveUnits);
	a.look = a.bar;
	return a;
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
	EnemyHealthTop from = EnemyHealthTop::Feet;  // what the bar's point is over
	float fraction = 0.0f;
	float metres = 0.0f;                // from the eyes
	NiPoint3 at{0.0f, 0.0f, 0.0f};    // the bar's point, world
	NiPoint3 look{0.0f, 0.0f, 0.0f};  // what the eyes' ray goes to, world
	float overFeetUnits = 0.0f;         // the bar's point over the feet, for the log
	UInt32 baseForm = 0;                // the base's form ID, for the log
};

struct EnemyHealthBar {
	UInt32 actor = 0;  // 0: free
	float fraction = 0.0f;
	float painted = -1.0f;   // the fraction last painted; -1 never
	UInt32 revision = 0;     // for the canvas: changed when painted anew
	float metres = 0.0f;
	NiPoint3 at{0.0f, 0.0f, 0.0f};
	NiPoint3 look{0.0f, 0.0f, 0.0f};
	EnemyHealthTop from = EnemyHealthTop::Feet;
	float linger = 0.0f;   // seconds not fighting (or dead)
	float seconds = 0.0f;  // how long the bar has been kept
	bool dead = false;
	// Since the health last moved (or the bar began on a hurt fighter);
	// kEnemyHealthLongAgo for a bar begun at full health.
	float sinceChange = 0.0f;
	bool sightKnown = false;  // the eyes' ray asked since the bar was last wanted
	bool inSight = false;     // its answer: nothing solid between the eyes and them
	bool downAtDeath = false;  // the bar was not up when its fighter died: it stays down
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

// ------------------------------------------------ when a bar tells something
// A bar shows while its fighter is hurt, for `showSeconds` after the health
// last moved, then fades out over kEnemyHealthFadeSeconds; the next move
// brings it back at once. The death is no move: a bar up when its fighter
// dies shows them empty for what is left of its time (and the two seconds'
// linger at most), one that was down stays down - a kill with one blow
// shows no bar. Hurt stands in for "the player has hit them at
// least once, any way" - a guard's blow or a trap counts as well.
inline constexpr float kEnemyHealthFadeSeconds = 0.5f;
// Health below this share of the base is hurt (a hair under full is not).
inline constexpr float kEnemyHealthHurtBelow = 0.999f;
// The health moved: by more than this share of the base.
inline constexpr float kEnemyHealthMoveStep = 1.0f / 512.0f;
// A bar begun at full health: its last move long ago.
inline constexpr float kEnemyHealthLongAgo = 1.0e6f;
// A wanted bar's eyes' ray every this many frames (a bar's slot sets its
// turn, so eight bars cast two rays a frame); a bar wanted anew casts at once.
inline constexpr UInt32 kEnemyHealthSightEveryFrames = 4;

inline bool EnemyHealthMoved(float was, float now) {
	const float d = was > now ? was - now : now - was;
	return d > kEnemyHealthMoveStep;
}

inline bool EnemyHealthHurt(const EnemyHealthBar& b) {
	return b.dead || !(b.fraction >= kEnemyHealthHurtBelow);
}

// The time's share of the bar's opacity: 1 within `showSeconds` of the last
// move, down to 0 over the fade after it, 0 when not hurt or nobody's.
inline float EnemyHealthTimeAlpha(const EnemyHealthBar& b, float showSeconds) {
	if (b.actor == 0 || !EnemyHealthHurt(b) || (b.dead && b.downAtDeath)) {
		return 0.0f;
	}
	if (!(b.sinceChange > showSeconds)) {
		return 1.0f;
	}
	const float t = (b.sinceChange - showSeconds) / kEnemyHealthFadeSeconds;
	return t >= 1.0f ? 0.0f : 1.0f - t;
}

// The bar's sight this frame. A bar not wanted (not hurt, its time over)
// forgets it and casts nothing, so it asks anew the moment it is wanted. A
// bar over no model (EnemyHealthTop::Feet) is out of sight - nothing is drawn
// there. Else the eyes' ray, `castClear(look)` - true when nothing solid lies
// between - when the bar's sight is not known yet or it is its turn. Answers
// whether the ray was cast.
template <class CastClear>
bool StepEnemyHealthSight(EnemyHealthBar& b, UInt32 slot, UInt32 frame, float showSeconds, CastClear&& castClear) {
	if (!(EnemyHealthTimeAlpha(b, showSeconds) > 0.0f)) {
		b.sightKnown = false;
		b.inSight = false;
		return false;
	}
	if (b.from == EnemyHealthTop::Feet) {
		b.sightKnown = true;
		b.inSight = false;
		return false;
	}
	if (b.sightKnown && (frame + slot) % kEnemyHealthSightEveryFrames != 0) {
		return false;
	}
	b.inSight = castClear(b.look);
	b.sightKnown = true;
	return true;
}

// How opaque the bar is now: its time's share while it is in sight, else 0
// (lost sight hides it at once).
inline float EnemyHealthBarAlpha(const EnemyHealthBar& b, float showSeconds) {
	return b.sightKnown && b.inSight ? EnemyHealthTimeAlpha(b, showSeconds) : 0.0f;
}

// What hides a fighter from the eyes: the first fixed thing on the ray
// (walls, the ground, doors, trees - the blade's own kinds, ContactKindOf),
// passing what is movable or ignored, and passing fixed bodies of the biped
// layer: a person's own bones never hide them, nor another person standing
// between (whether the living's bones are fixed bodies in the world is not
// measured; the ragdolls of the dead are movable). The hit's fraction is
// along the whole ray; no hit when nothing hides them.
template <class World>
BladeRayHit FirstSightBlocker(World& world, const NiPoint3& from, const NiPoint3& to) {
	const NiPoint3 ray = to - from;
	const float length = math::Sqrt(ray.LengthSquared());
	if (!(length > 1e-4f)) {
		return BladeRayHit{};
	}
	const NiPoint3 dir = ray * (1.0f / length);
	float startAt = 0.0f;
	for (UInt32 pass = 0; pass < kBladeCastPasses; ++pass) {
		BladeRayHit h = world.Cast(from + dir * startAt, to);
		if (!h.hit) {
			return BladeRayHit{};
		}
		const float at = startAt + h.fraction * (length - startAt);
		h.fraction = at / length;
		if (h.kind == ContactKind::Fixed && h.layer != kContactLayerBiped) {
			return h;
		}
		startAt = at + kBladePastUnits;
		if (startAt >= length) {
			return BladeRayHit{};
		}
	}
	return BladeRayHit{};
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
		b.sinceChange = b.sinceChange < kEnemyHealthLongAgo ? b.sinceChange + dt : kEnemyHealthLongAgo;
		if (found != nullptr) {
			const float fraction = found->dead ? 0.0f : found->fraction;
			if (found->dead && !b.dead) {
				// Down at the death (not hurt, or out of sight): it stays down.
				b.downAtDeath = !EnemyHealthHurt(b) || (b.sightKnown && !b.inSight);
			}
			// A death is not a move: the bar keeps the time of the last
			// move before it, so a kill with one blow never brings it up and
			// a faded one stays down (the tester, 2026-10-26).
			if (!found->dead && EnemyHealthMoved(b.fraction, fraction)) {
				b.sinceChange = 0.0f;
			}
			b.at = found->at;
			b.look = found->look;
			b.from = found->from;
			b.metres = found->metres;
			b.dead = found->dead;
			b.fraction = fraction;
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
		b.look = seen[best].look;
		b.from = seen[best].from;
		b.metres = seen[best].metres;
		// Hurt before the fight (the blow that began it): shown at once.
		b.sinceChange = EnemyHealthHurt(b) ? 0.0f : kEnemyHealthLongAgo;
		b.painted = b.fraction;
		b.revision = 1;
		used[slot] = true;
		++v.started;
		++v.shown;
	}
	return v;
}

// ------------------------------------------- the game's own bar, its frames
// The tester (2026-10-13): "statt unserem eigenen balken den von vanilla
// rendern über den gegnern". HUDReticle's enemy health is a model, not an
// image: hud_reticle.xml's <nif> "hudreticle_enemy_health" loads
// meshes/menus/enemy health bar/health_bar01.nif (read 2026-10-13 with
// pyffi) - one quad, 128 by 32 units, with a NiFlipController on its
// texturing property holding 40 textures,
// textures/menus/misc/sequence_dds/000001..000040.dds (128 by 32, DXT5, no
// mipmaps): a golden arc, the widest in the first (79 of 128 columns) and a
// stub in the last (11) - the first is full health, the last empty; the
// HUD runs them through its sequence "Health_Bar" (42 float keys, one a
// frame at 30 a second) from the tile's user traits. OBVR reads the frames
// off those textures (game/EnemyHealthModel.h) and paints the one for the
// fraction on each bar's overlay (ui/EnemyHealthPainter.h).

// The frame for a fraction of health: 0 the fullest, frames - 1 the
// emptiest, the nearest in between. Not a number counts as full.
inline UInt32 EnemyHealthFrameIndex(float fraction, UInt32 frames) {
	if (frames < 2) {
		return 0;
	}
	float f = fraction == fraction ? fraction : 1.0f;
	f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
	const float last = static_cast<float>(frames - 1);
	const UInt32 index = static_cast<UInt32>((1.0f - f) * last + 0.5f);
	return index > frames - 1 ? frames - 1 : index;
}

// ------------------------------- a frame's Direct3D texture, by its address
// Where a NiSourceTexture keeps its IDirect3DTexture9 (Oblivion.exe
// 1.2.0.416, read 2026-10-14): its renderer data at +0x24 - the shader
// property's set-up reads it there (0x007DA8A9) - is a
// NiDX9SourceTextureData (vtable 0x00A883C4), and that class's slot +0x24
// (0x007609C0) takes the Direct3D texture, asks it for its level count
// (kept at +0x5C), its type and each level's size (the first at +0x54 and
// +0x58), copies the pixel format to +0x0C and keeps the texture at +0x50
// (0x00760A7B). The probe of 2026-10-13 found it there at run time.
//
// Nothing else in the renderer data is followed. The pixel format's
// D3DFORMAT lies at +0x18, for these frames the four characters "DXT5",
// 0x35545844: four-aligned and inside mem::LooksLikeObjectAddress's range, a
// pointer by its looks only. The scan that came before followed every dword
// from +0x04 on and met it before the texture; Windows read through it
// unharmed from 2026-10-13 on, the address happening to be readable there,
// and under Proton on 2026-10-14 nothing was mapped at it and the game went
// down with the session's first enemy bar (docs/hud-on-hands-spec.md).
inline constexpr UInt32 kTextureRendererDataOffset = 0x24;
inline constexpr UInt32 kRendererDataD3DTextureOffset = 0x50;

enum class D3DTextureResult : UInt8 {
	Found,
	NoVtable,        // IDirect3DTexture9's vtable unknown, nothing to tell it by
	NoRendererData,  // no texture object, or it has no renderer data
	NoTexture,       // nothing that looks like an object at +0x50
	OtherObject,     // an object at +0x50 whose vtable is not the texture's
};

struct D3DTextureLookup {
	D3DTextureResult result = D3DTextureResult::NoRendererData;
	UInt32 texture = 0;       // the IDirect3DTexture9, when found
	UInt32 rendererData = 0;  // the renderer data, once read
};

// The IDirect3DTexture9 behind a NiSourceTexture: the renderer data's +0x50,
// taken when its first dword is `textureVtable`, the vtable every
// IDirect3DTexture9 of the game's device shares. `read(address)` reads one
// dword of the game's memory; it is asked for the renderer data, the dword at
// +0x50 and that object's vtable, each only once what led to it looks like an
// object, and for nothing else.
template <typename ReadFn>
D3DTextureLookup LookUpD3DTexture(UInt32 texture, UInt32 textureVtable, ReadFn read) {
	D3DTextureLookup r;
	if (textureVtable == 0) {
		r.result = D3DTextureResult::NoVtable;
		return r;
	}
	if (!mem::LooksLikeObjectAddress(texture)) {
		return r;
	}
	r.rendererData = read(texture + kTextureRendererDataOffset);
	if (!mem::LooksLikeObjectAddress(r.rendererData)) {
		r.rendererData = 0;
		return r;
	}
	const UInt32 candidate = read(r.rendererData + kRendererDataD3DTextureOffset);
	if (!mem::LooksLikeObjectAddress(candidate)) {
		r.result = D3DTextureResult::NoTexture;
		return r;
	}
	if (read(candidate) != textureVtable) {
		r.result = D3DTextureResult::OtherObject;
		return r;
	}
	r.result = D3DTextureResult::Found;
	r.texture = candidate;
	return r;
}

// The probe's line when no texture was found: "<what> <address>: +00=...
// +04=..." with the dwords as they are - none of them followed, for a format
// or a size can look like a pointer. As many dwords as fit whole; always
// terminated. Returns the dwords written.
inline UInt32 FormatProbeDwords(char* out, UInt32 size, const char* what, UInt32 address, const UInt32* dwords,
                                UInt32 count) {
	if (out == nullptr || size == 0) {
		return 0;
	}
	const int label = std::snprintf(out, size, "%s %08X:", what, address);
	if (label < 0 || static_cast<UInt32>(label) >= size) {
		return 0;
	}
	UInt32 at = static_cast<UInt32>(label);
	UInt32 written = 0;
	for (UInt32 i = 0; i < count; ++i) {
		char entry[24];
		const int n = std::snprintf(entry, sizeof(entry), " +%02X=%08X", i * 4u, dwords[i]);
		if (n < 0 || at + static_cast<UInt32>(n) >= size) {
			break;
		}
		std::memcpy(out + at, entry, static_cast<size_t>(n) + 1);
		at += static_cast<UInt32>(n);
		++written;
	}
	return written;
}

}  // namespace obvr::game
