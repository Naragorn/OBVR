#pragma once

// The head comes off (docs/dismemberment-spec.md; the tester, 2026-10-25:
// "decapitations ... B erstmal nur spec"; Nadi, 2026-10-30: "go"). Health
// decides whether someone dies, the blade only how: a blade or an axe swept
// fast enough through the neck of someone dead - a corpse, or the one the
// blow just killed - cuts the head off. The living keep their parts
// (CutKills=1 drops that, never for the essential).
//
// The cut itself (game/Dismember.cpp) releases the head's one ragdoll joint
// to Spine2 and redirects bone pointers in the skins, as the arm stump does:
// in the body's shapes the head bone collapses at the neck, in the head's
// shapes everything but the head collapses under it. Pure, covered by
// dismember_test.

#include <cmath>

#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// TESObjectWEAP's type (+0x90): blades 0 and 1, blunt 2 and 3 (axes among
// them), staff 4, bow 5; -1 none.
inline constexpr SInt32 kCutWeaponBladeOneHand = 0;
inline constexpr SInt32 kCutWeaponBladeTwoHand = 1;
inline constexpr SInt32 kCutWeaponBluntOneHand = 2;
inline constexpr SInt32 kCutWeaponBluntTwoHand = 3;
inline constexpr SInt32 kCutWeaponStaff = 4;
inline constexpr SInt32 kCutWeaponBow = 5;

inline bool CutPathHasAxe(const char* path) {
	if (path == nullptr) {
		return false;
	}
	for (const char* p = path; p[0] != '\0'; ++p) {
		const char a = static_cast<char>(p[0] | 0x20);
		if (a != 'a' || p[1] == '\0') {
			continue;
		}
		const char x = static_cast<char>(p[1] | 0x20);
		if (x == 'x' && p[2] != '\0' && static_cast<char>(p[2] | 0x20) == 'e') {
			return true;
		}
	}
	return false;
}

// An edge: a blade, or a blunt weapon whose model is an axe - the engine has
// no axe type (read 2026-10-30: "axe" in the model path catches every vanilla
// axe; seven enchanted ones are typed Blade, caught either way).
inline bool WeaponHasEdge(SInt32 type, const char* modelPath) {
	if (type == kCutWeaponBladeOneHand || type == kCutWeaponBladeTwoHand) {
		return true;
	}
	return (type == kCutWeaponBluntOneHand || type == kCutWeaponBluntTwoHand) && CutPathHasAxe(modelPath);
}

// Whether the blade, from where it was the frame before to where it is now,
// passed through the neck (Spine2's origin to the head's, `radius` round):
// the blade now, and the paths of nine points along it - the swept face of
// the blade, as far as a neck's width can slip between them.
inline constexpr int kNeckSweepPoints = 9;

inline bool NeckTouched(const NiPoint3& neckLow, const NiPoint3& neckHigh, float radius, const NiPoint3& gripBefore,
                        const NiPoint3& tipBefore, const NiPoint3& gripNow, const NiPoint3& tipNow) {
	if (SegmentSegmentDistance(gripNow, tipNow, neckLow, neckHigh) <= radius) {
		return true;
	}
	for (int i = 0; i < kNeckSweepPoints; ++i) {
		const float t = static_cast<float>(i) / static_cast<float>(kNeckSweepPoints - 1);
		const NiPoint3 before = gripBefore + (tipBefore - gripBefore) * t;
		const NiPoint3 now = gripNow + (tipNow - gripNow) * t;
		if (SegmentSegmentDistance(before, now, neckLow, neckHigh) <= radius) {
			return true;
		}
	}
	return false;
}

struct CutSettings {
	bool enabled = true;          // [Hands] Dismemberment (on since Nadi tried it in the game, 2026-10-31)
	// The blade's tip, m/s (Nadi, 2026-11-05: cuts came too easily at 2.5):
	// the neck [Hands] DismemberMinSpeed, a limb [Hands] DismemberLimbMinSpeed,
	// a two-handed blade that share of either ([Hands] DismemberTwoHandedShare).
	float minSpeedMetres = 5.0f;
	float limbMinSpeedMetres = 6.0f;
	float twoHandedShare = 0.8f;
	float neckRadiusUnits = 6.0f;
	bool cutKills = false;        // [Hands] DismemberCutKills: the living too
	// The blood (game/Blood.h): the engine's own spray at the cut, bloodPulses
	// spurts bloodPulseSeconds apart from the body's neck, the first with one
	// from the head and a decal under the neck. [Hands] DismemberBlood.
	bool blood = true;
	UInt32 bloodPulses = 3;
	float bloodPulseSeconds = 0.35f;
	// The wound caps: vanilla's zombie neck plug on the stump and under the head.
	// [Hands] DismemberCaps.
	bool caps = true;
	// The cut's sound: the blade in flesh and the bone, at the part.
	// [Hands] DismemberSound.
	bool sound = true;
	// The most cut parts standing at once (CutLimitFor: kCutsLeast..kCutsMost,
	// default all the slots). [Hands] DismemberMostCuts.
	int most = 128;
};

// The sounds a cut makes (Oblivion.esm's SOUN forms, read 2026-11-05):
// WPNHitBladeFlesh (fx\wpn\weaponhit\bladeflesh\) and PHYDamageBone
// (fx\phy\combat\damage\bone\) - none when off, none for a cut done again
// after a load (its first cut made them). Answers how many it wrote.
inline constexpr UInt32 kSoundCutFlesh = 0x0018B9F1;
inline constexpr UInt32 kSoundCutBone = 0x00025417;
inline constexpr UInt32 kCutSoundsMost = 2;

inline UInt32 CutSoundsFor(bool wanted, bool redo, UInt32* out) {
	if (!wanted || redo) {
		return 0;
	}
	out[0] = kSoundCutFlesh;
	out[1] = kSoundCutBone;
	return 2;
}

// The essential flag in an actor base's flags (xOBSE GameForms.h:
// TESActorBase's actorBaseData at +0x24, its flags at +0x04,
// kFlag_IsEssential 0x02) - for NPCs and creatures alike.
inline constexpr UInt32 kEssentialFlagsOffset = 0x24 + 0x04;
inline constexpr UInt32 kActorBaseFlagEssential = 0x02;

inline bool ActorBaseFlagsEssential(UInt32 flags) { return (flags & kActorBaseFlagEssential) != 0; }

struct CutInput {
	bool enabled = false;
	bool alreadyCut = false;
	bool dead = false;       // a corpse, or killed by this blow
	bool essential = false;
	bool edge = false;
	bool onNeck = false;
	bool limb = false;        // a hand, an arm or a leg, not the head
	bool twoHanded = false;   // a two-handed blade
	float speedMetres = 0.0f;
};

// The speed a cut wants (CutSettings): the neck's or a limb's, a two-handed
// blade's share of it (a share outside 0..1 counts as none).
inline float CutSpeedNeeded(const CutSettings& s, bool limb, bool twoHanded) {
	const float base = limb ? s.limbMinSpeedMetres : s.minSpeedMetres;
	const bool share = twoHanded && s.twoHandedShare > 0.0f && s.twoHandedShare <= 1.0f;
	return share ? base * s.twoHandedShare : base;
}

enum class CutVerdict : UInt8 { Cut, Off, AlreadyCut, Living, Essential, NoEdge, NotOnNeck, TooSlow };

inline const char* CutVerdictName(CutVerdict v) {
	switch (v) {
		case CutVerdict::Cut: return "cut";
		case CutVerdict::Off: return "off";
		case CutVerdict::AlreadyCut: return "that part is off already";
		case CutVerdict::Living: return "alive - the living keep their parts";
		case CutVerdict::Essential: return "essential";
		case CutVerdict::NoEdge: return "no edge";
		case CutVerdict::NotOnNeck: return "not the neck";
		case CutVerdict::TooSlow: return "too slow";
	}
	return "?";
}

inline CutVerdict DecideCut(const CutSettings& s, const CutInput& in) {
	if (!in.enabled) {
		return CutVerdict::Off;
	}
	if (in.alreadyCut) {
		return CutVerdict::AlreadyCut;
	}
	// The essential never lose a part: at no health they lie unconscious and
	// read dead (camera/FrameLogic.h), so this comes before the death.
	if (in.essential) {
		return CutVerdict::Essential;
	}
	if (!in.dead && !s.cutKills) {
		return CutVerdict::Living;
	}
	if (!in.edge) {
		return CutVerdict::NoEdge;
	}
	if (!in.onNeck) {
		return CutVerdict::NotOnNeck;
	}
	if (in.speedMetres < CutSpeedNeeded(s, in.limb, in.twoHanded)) {
		return CutVerdict::TooSlow;
	}
	return CutVerdict::Cut;
}

// The killing blow. The engine's hit lands at the touch and its death comes
// a few frames later, so a living neck the blade went through - with
// everything else a cut wants - waits for the death: dead within
// kKillingBlowSeconds, the head comes off; alive past it, or dead only
// after it (another's blow), the touch is forgotten.
inline constexpr float kKillingBlowSeconds = 0.4f;

inline bool ArmsKillingBlow(const CutSettings& s, const CutInput& in) {
	if (in.dead || s.cutKills || in.essential) {
		return false;
	}
	CutInput asDead = in;
	asDead.dead = true;
	return DecideCut(s, asDead) == CutVerdict::Cut;
}

struct PendingCut {
	bool armed = false;
	UInt32 actor = 0;
	float age = 0.0f;
	NiPoint3 push{0.0f, 0.0f, 0.0f};
};

enum class PendingStep : UInt8 { None, Wait, Cut, Expired };

inline void ArmPendingCut(PendingCut& p, UInt32 actor, const NiPoint3& push) {
	p.armed = true;
	p.actor = actor;
	p.age = 0.0f;
	p.push = push;
}

inline PendingStep StepPendingCut(PendingCut& p, float dt, bool nowDead) {
	if (!p.armed) {
		return PendingStep::None;
	}
	p.age += dt > 0.0f ? dt : 0.0f;
	if (p.age > kKillingBlowSeconds) {
		p.armed = false;
		return PendingStep::Expired;
	}
	if (nowDead) {
		p.armed = false;
		return PendingStep::Cut;
	}
	return PendingStep::Wait;
}

// A blunt blow - a kick, a shield bash, a pommel or the half-sword's hilt
// (game::KickStrike) - on the one a blade's touch left waiting takes the
// death from the blade: the waiting cut is forgotten (Nadi, 2026-11-01:
// "kicks sollten niemanden enthaupten können" - a blade that grazed the neck
// a moment before the kick that killed took the head).
inline void ForgetPendingCutFor(PendingCut& p, UInt32 actor) {
	if (p.armed && p.actor == actor) {
		p.armed = false;
	}
}

// And for a moment after it the blade cuts nothing of theirs either: the one a
// kick killed falls with the blade still moving at their neck, and a cut of
// the dead then would be the kick's. kBluntGuardSeconds from the blunt blow.
inline constexpr float kBluntGuardSeconds = 0.5f;

inline bool BluntGuardHolds(UInt32 guardActor, float ageSeconds, UInt32 actor) {
	return actor != 0 && actor == guardActor && ageSeconds < kBluntGuardSeconds;
}

// The killing strike: the engine's hit kills in the call (the strike's
// health line, 2026-10-30: 74 -> -401), and the body flies off before the
// blade's sweep reaches the neck - so the cut is decided at the touch, by the
// capsule the blade met: the neck takes the head, nothing else does.
inline bool StrikeKilled(float healthBefore, float healthAfter) {
	return healthBefore == healthBefore && healthAfter == healthAfter && healthBefore > 0.0f && healthAfter <= 0.0f;
}

inline bool PartTakesHead(BodyPart part) { return part == BodyPart::Neck; }

// A blunt killing blow to the head (no edge, the head's capsule touched): nothing
// is cut, but the head is crushed - Walkers is told (kHeadCrushed), so a
// walker ends as by a cut (Nadi, 2026-10-25; "5. go", 2026-10-31). Not when
// the head is off already.
inline bool TellsHeadCrushed(bool edge, BodyPart part, bool alreadyCut) {
	return !edge && part == BodyPart::Head && !alreadyCut;
}

// The capsules do not tell the neck well (a slash at its height meets the
// shoulder first, a thrust at it the torso's top: the harness, 2026-10-30),
// so the killing strike is also cut when the blade's sweep - the frame before
// to now, and on the way it was going for kKillingSweepSeconds more, the time
// the flung body took from it - passes through the neck as it stood the frame
// before the blow.
inline constexpr float kKillingSweepSeconds = 0.08f;

inline bool KillingSweepMeetsNeck(const NiPoint3& neckLow, const NiPoint3& neckHigh, float radius,
                                  const NiPoint3& gripBefore, const NiPoint3& tipBefore, const NiPoint3& gripNow,
                                  const NiPoint3& tipNow, float frameSeconds, float aheadSeconds) {
	if (!(frameSeconds > 0.0f)) {
		return false;
	}
	const float scale = aheadSeconds > 0.0f ? aheadSeconds / frameSeconds : 0.0f;
	const NiPoint3 gripOn = gripNow + (gripNow - gripBefore) * scale;
	const NiPoint3 tipOn = tipNow + (tipNow - tipBefore) * scale;
	return NeckTouched(neckLow, neckHigh, radius, gripBefore, tipBefore, gripOn, tipOn);
}

// The head is sent off the blade's way at half its speed, at most
// kCutPushMostUnits (5 m/s; a harness thrust's 1300 units/s threw it 1040
// units, 2026-10-30).
inline constexpr float kCutPushMostUnits = 350.0f;

inline NiPoint3 CutPush(const NiPoint3& push) {
	const float sq = push.LengthSquared();
	if (!(sq > kCutPushMostUnits * kCutPushMostUnits)) {
		return sq == sq ? push : NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return push * (kCutPushMostUnits / std::sqrt(sq));
}

// Whether the next spurt is due: `done` spurts so far, `ageSeconds` since the
// cut. The first at once, then every bloodPulseSeconds, bloodPulses in all.
inline bool BloodPulseDue(const CutSettings& s, float ageSeconds, UInt32 done) {
	if (!s.blood || done >= s.bloodPulses || !(ageSeconds >= 0.0f)) {
		return false;
	}
	const float step = s.bloodPulseSeconds > 0.0f ? s.bloodPulseSeconds : 0.0f;
	return ageSeconds >= step * static_cast<float>(done);
}

// The spray's way: from `from` toward `to` (the body's neck toward where the
// head sat), unit length; straight up when there is none to tell.
inline NiPoint3 SprayDirection(const NiPoint3& from, const NiPoint3& to) {
	const NiPoint3 d = to - from;
	const float sq = d.LengthSquared();
	if (!(sq > 1e-6f)) {
		return NiPoint3{0.0f, 0.0f, 1.0f};
	}
	return d * (1.0f / std::sqrt(sq));
}

// Telling Oblivion Walkers (its spec 4.17 DieOnlyBeheaded; the format agreed
// with the Walkers session 2026-10-30): 'WGDM' one word, whether OBVR cuts -
// said after PostPostLoad and at the world frames until Walkers takes it
// (it listens to OBVR from its PostLoad), and again when the setting changes;
// 'WGHD' at each cut, the reference's form id and the kind. Walkers ends the
// walker whether it is dead yet or not.
inline constexpr UInt32 kWalkersDismemberMessage = 0x4D444757;  // 'WGDM'
inline constexpr UInt32 kWalkersHeadMessage = 0x44484757;       // 'WGHD'
inline constexpr UInt32 kHeadCut = 1;
inline constexpr UInt32 kHeadCrushed = 2;  // a blunt killing blow to the head (TellsHeadCrushed)

// 'WGPS' (agreed 2026-10-30; Nadi: "Go"): the reference the player's power
// strike is about to hit - sent right before the engine's hit, every power
// strike, so Walkers can end a walker a power attack kills (its spec 4.17
// point 6) - xOBSE's IsPowerAttacking looks for a running AttackPower
// sequence, and OBVR's strike plays none.
inline constexpr UInt32 kWalkersPowerStrikeMessage = 0x53504757;  // 'WGPS'

inline bool TellsPowerStrike(bool heavy, UInt32 refId) { return heavy && refId != 0; }

struct WalkersHeadWords {
	UInt32 refId = 0;
	UInt32 kind = 0;
};

inline WalkersHeadWords HeadMessageFor(UInt32 refId, UInt32 kind) {
	WalkersHeadWords w;
	w.refId = refId;
	w.kind = kind;
	return w;
}

struct WalkersDismemberSay {
	bool taken = false;
	bool value = false;
};

inline bool WalkersDismemberDue(const WalkersDismemberSay& say, bool enabled) {
	return !say.taken || say.value != enabled;
}

inline void NoteWalkersDismemberSaid(WalkersDismemberSay& say, bool enabled, bool taken) {
	say.value = enabled;
	say.taken = taken;
}

// The body's shapes give up the head bone and what hangs under it; the
// head's shapes everything else.
inline bool SwapBoneOnBodySide(bool isHead, bool underHead) { return isHead || underHead; }
inline bool SwapBoneOnHeadSide(bool isHead, bool underHead) { return !isHead && !underHead; }

// A hand's shapes share a mesh with the other hand (the NPCs' hands are one
// shape, both hands' bones in it: "handRM", 16 of 36 bones the cut hand's,
// the harness 2026-10-31 20:21). On the part's side of a hand cut only the
// bones that hold the cut hand and not the other one - the forearm and up the
// arm, not the spine both hang from - go to the part; the other hand's bones
// stay. A head's part side keeps the rule above (everything not the head's).
// And the bones under the stump's own bone that are not the part's - the
// forearm's twist bone beside the hand, the upper arm's beside the forearm:
// the hand's mesh weighs the wrist on Bip01 L ForearmTwist too, and left on
// the body it stretched the wrist after the flying hand to the horizon
// (Nadi, 2026-11-05).
// And the bones both limbs hang from (the spine, the pelvis): with the
// other limb in the same shape (handRM, a pair of boots) they stay, its
// middle is the body's; in a shape of the cut limb alone (a sleeve, an arm's
// own mesh) they go to the part too - left on the body they drew the flying
// limb's seam back to the spine (Nadi's Windows run of 2026-11-10: arms cut at
// the shoulder, "Body02:0(head, 5/7)" - 5 of its 7 bones the arm's, Clavicle
// sent, the spine left).
// `inOther`: the bone is the other limb's - its match, under it, or holding
// it and not the cut one. `shapeHasOther`: any bone of the shape is.
// `underStump`: on the cut limb's own side - under the nearest bone holding
// the part that does not hold the other limb (the twists beside the chain).
inline bool SwapBoneOnPartSide(bool headCut, bool isPart, bool underPart, bool holdsPart, bool holdsOther,
                               bool underStump, bool inOther = false, bool shapeHasOther = true) {
	if (headCut) {
		return SwapBoneOnHeadSide(isPart, underPart);
	}
	if (isPart || underPart || inOther) {
		return false;
	}
	if ((holdsPart && !holdsOther) || underStump) {
		return true;
	}
	return holdsPart && holdsOther && !shapeHasOther;
}

// A bone of a part's side shape the swap leaves on the body that is not the
// other limb's and not a middle shared with it: it stays where the body is
// while the part flies off - the stretch. Counted per cut for the log
// ("left on the body"), so a harness run can show none.
inline bool PartSideBoneLeftBehind(bool headCut, bool isPart, bool underPart, bool holdsPart, bool holdsOther,
                                   bool underStump, bool inOther, bool shapeHasOther) {
	if (headCut || isPart || underPart || inOther) {
		return false;
	}
	if (SwapBoneOnPartSide(false, false, false, holdsPart, holdsOther, underStump, false, shapeHasOther)) {
		return false;
	}
	return !(holdsPart && holdsOther && shapeHasOther);
}

// A shape outside the head node is the head's when a third or more of its
// bones are the head's - the head, what hangs under it, Neck and Neck1 -
// whatever the pose (the bound's middle against the head and Spine2 took a
// standing beggar's shirt for the head's, 2026-10-30). Measured that day on
// the beggar: FaceGenFace 2 of 5, its shirt's two shapes 2 of 10 and 2 of 19,
// its feet 2 of 20.
inline bool ShapeIsHeadSide(UInt32 headBones, UInt32 bones) { return bones > 0 && headBones * 3 >= bones; }

// The engine functions the wound caps call (the model loader, the clone and
// the two updates): their first bytes as read, or another plugin's jump at
// the entry - a detour, which still runs the function when called, as the
// engine itself calls it - or something else (another game version, a patch
// of another shape), which is not called. Nadi's Windows logs of 2026-11-10
// said "NOT as read - no caps" in every session: no limb or head got a cap
// there, and the stumps were open ("in den Körper schauen"); on Linux, with
// fewer plugins, the caps were on.
enum class CapEntry : UInt8 { AsRead, Detoured, Unknown };

// A cut body's skins read again now and then: a shape the engine draws after
// the cut - a blood decal of a later hit, clothes put on or taken off - has
// its own bone pointers, the part's real bones among them, and stretched to
// the flying part (Nadi, 2026-11-10: hands and legs "manchmal" stretched).
// Every kCutRescanSeconds a standing cut's skins are swapped again; a shape
// swapped already is left as it is. True when one is due (and the clock
// started over); a negative or not-a-number frame time counts nothing.
inline constexpr float kCutRescanSeconds = 0.5f;

inline bool CutRescanDue(float& seconds, float dtSeconds) {
	if (dtSeconds > 0.0f) {
		seconds += dtSeconds;
	}
	if (!(seconds >= kCutRescanSeconds)) {
		return false;
	}
	seconds = 0.0f;
	return true;
}

inline CapEntry CapEntryOf(bool bytesAsRead, bool foreignJump) {
	return bytesAsRead ? CapEntry::AsRead : (foreignJump ? CapEntry::Detoured : CapEntry::Unknown);
}

inline bool CapEntryCallable(CapEntry e) { return e != CapEntry::Unknown; }

// The heads cut, kept over a save and load and over a cell left and come back
// to (docs/dismemberment-spec.md section 3): the engine rebuilds an actor's 3D
// and its ragdoll with the head on, so OBVR keeps the cut actors' form ids
// (in its co-save, 'OBCH') and cuts again once the actor's 3D has stood
// kRedoAfterSeconds - the ragdoll built - with no push, no blood, no
// message. The newest kCutMemoryMost are kept.
inline constexpr UInt32 kCutMemoryMost = 64;
inline constexpr float kRedoAfterSeconds = 1.0f;

// The parts cut at once (game/Dismember.cpp's slots). A slot cannot be freed
// under a body that still stands - its skins point at the slot's own nodes -
// so the count is the limit: 16 refused cuts at a gate where walkers killed
// 90 guards (Walkers' gate-guards run, 2026-11-09); 128 (Nadi, 2026-11-09:
// option 3) and never fewer than the co-save remembers, so every remembered
// cut can stand again after a load. About 0.9 KB a slot.
inline constexpr UInt32 kCutsMost = 128;

// The most cuts at once the player asks for ([Hands] DismemberMostCuts, Nadi,
// 2026-11-09): kept within kCutsLeast (the 16 slots OBVR had) and kCutsMost
// (the slots there are); clamped says it was moved there.
inline constexpr UInt32 kCutsLeast = 16;
struct CutLimit {
	UInt32 most = kCutsMost;
	bool clamped = false;
};
inline CutLimit CutLimitFor(int asked) {
	if (asked < static_cast<int>(kCutsLeast)) {
		return CutLimit{kCutsLeast, true};
	}
	if (asked > static_cast<int>(kCutsMost)) {
		return CutLimit{kCutsMost, true};
	}
	return CutLimit{static_cast<UInt32>(asked), false};
}

// Room for one more cut: fewer standing than the limit. A limit lowered under
// the cuts standing frees none of them - a standing body's skins point at its
// slot's own nodes - so new cuts wait until enough have gone.
inline bool CutRoom(UInt32 standing, UInt32 most) { return standing < most; }

// StepCuts' own time, said once every framesPerLine frames: the frames, the
// microseconds in all and the longest, the most cuts standing in them.
struct CutCostWindow {
	UInt32 frames = 0;
	double sumMicros = 0.0;
	double maxMicros = 0.0;
	UInt32 mostLive = 0;
};

// One frame added; true when the window is full and its line is due (the
// caller says it and starts a new window). A time that is no number or below
// zero is not counted; framesPerLine 0 is never due.
inline bool AddCutCost(CutCostWindow& w, double micros, UInt32 live, UInt32 framesPerLine) {
	if (!(micros >= 0.0)) {
		return false;
	}
	++w.frames;
	w.sumMicros += micros;
	w.maxMicros = micros > w.maxMicros ? micros : w.maxMicros;
	w.mostLive = live > w.mostLive ? live : w.mostLive;
	return framesPerLine > 0 && w.frames >= framesPerLine;
}

inline double CutCostMean(const CutCostWindow& w) { return w.frames > 0 ? w.sumMicros / w.frames : 0.0; }

// The parts that come off (Nadi, 2026-10-31: "7 wenn rest steht dann go" - the
// hands after the head): the head at the neck, a hand at its wrist.
// The limbs at their joints after them (Nadi, 2026-11-05: "go 4"): the
// forearm at the elbow, the whole arm at the shoulder, the lower leg at the
// knee. The values are the co-save's (CutsOfRecord): never renumbered.
enum class CutPart : UInt8 {
	Head = 0,
	LeftHand = 1,
	RightHand = 2,
	LeftForearm = 3,
	RightForearm = 4,
	LeftArm = 5,
	RightArm = 6,
	LeftLowerLeg = 7,
	RightLowerLeg = 8,
};
inline constexpr UInt32 kCutPartCount = 9;
inline constexpr UInt32 kCutPartCountHands = 3;  // the parts a version 2 record knows

inline const char* CutPartName(CutPart p) {
	switch (p) {
	case CutPart::Head: return "head";
	case CutPart::LeftHand: return "left hand";
	case CutPart::RightHand: return "right hand";
	case CutPart::LeftForearm: return "left forearm";
	case CutPart::RightForearm: return "right forearm";
	case CutPart::LeftArm: return "left arm";
	case CutPart::RightArm: return "right arm";
	case CutPart::LeftLowerLeg: return "left lower leg";
	case CutPart::RightLowerLeg: return "right lower leg";
	default: return "part";
	}
}

inline const char* CutJointName(CutPart p) {
	switch (p) {
	case CutPart::LeftHand:
	case CutPart::RightHand: return "wrist";
	case CutPart::LeftForearm:
	case CutPart::RightForearm: return "elbow";
	case CutPart::LeftArm:
	case CutPart::RightArm: return "shoulder";
	case CutPart::LeftLowerLeg:
	case CutPart::RightLowerLeg: return "knee";
	case CutPart::Head:
	default: return "neck";
	}
}

inline bool IsLimbCut(CutPart p) {
	return static_cast<UInt8>(p) >= static_cast<UInt8>(CutPart::LeftForearm) &&
	       static_cast<UInt8>(p) < kCutPartCount;
}

// The bones of a cut: the part's own bone (it and what hangs under it come
// off), the ragdoll body its joint holds it to (the joint released; read from
// the vanilla skeleton.nif's constraints, 2026-11-05: UpperArm-Spine2,
// Forearm-UpperArm, Hand-Forearm, Calf-Thigh, Head-Spine2), the bone the
// stump's collapsed vertices follow, the matching part of the other side
// (whose bones a shared shape keeps: NPCs' hands are one shape) and, for a
// limb, the next bone down the part (JointSegment's direction).
struct CutPartBones {
	const char* part;
	const char* joint;
	const char* stump;
	const char* other;
	const char* next;
};

inline CutPartBones BonesOfCut(CutPart p) {
	switch (p) {
	case CutPart::LeftHand: return {"Bip01 L Hand", "Bip01 L Forearm", "Bip01 L Forearm", "Bip01 R Hand", nullptr};
	case CutPart::RightHand: return {"Bip01 R Hand", "Bip01 R Forearm", "Bip01 R Forearm", "Bip01 L Hand", nullptr};
	case CutPart::LeftForearm:
		return {"Bip01 L Forearm", "Bip01 L UpperArm", "Bip01 L UpperArm", "Bip01 R Forearm", "Bip01 L Hand"};
	case CutPart::RightForearm:
		return {"Bip01 R Forearm", "Bip01 R UpperArm", "Bip01 R UpperArm", "Bip01 L Forearm", "Bip01 R Hand"};
	case CutPart::LeftArm:
		return {"Bip01 L UpperArm", "Bip01 Spine2", "Bip01 L Clavicle", "Bip01 R UpperArm", "Bip01 L Forearm"};
	case CutPart::RightArm:
		return {"Bip01 R UpperArm", "Bip01 Spine2", "Bip01 R Clavicle", "Bip01 L UpperArm", "Bip01 R Forearm"};
	case CutPart::LeftLowerLeg:
		return {"Bip01 L Calf", "Bip01 L Thigh", "Bip01 L Thigh", "Bip01 R Calf", "Bip01 L Foot"};
	case CutPart::RightLowerLeg:
		return {"Bip01 R Calf", "Bip01 R Thigh", "Bip01 R Thigh", "Bip01 L Calf", "Bip01 R Foot"};
	case CutPart::Head:
	default: return {"Bip01 Head", "Bip01 Spine2", "Bip01 Neck1", nullptr, nullptr};
	}
}

// Which chain of the body a part is on: 1 the left arm, 2 the right arm, 3 the
// left leg, 4 the right leg, 0 the head. Two parts on one chain clash: the
// hand off the forearm cannot come off again with the forearm, and a forearm
// gone leaves no arm to cut whole - one cut per chain.
inline UInt32 CutChainOf(CutPart p) {
	switch (p) {
	case CutPart::LeftHand:
	case CutPart::LeftForearm:
	case CutPart::LeftArm: return 1;
	case CutPart::RightHand:
	case CutPart::RightForearm:
	case CutPart::RightArm: return 2;
	case CutPart::LeftLowerLeg: return 3;
	case CutPart::RightLowerLeg: return 4;
	case CutPart::Head:
	default: return 0;
	}
}

inline bool CutPartsClash(CutPart a, CutPart b) { return a == b || (CutChainOf(a) != 0 && CutChainOf(a) == CutChainOf(b)); }

// The wrist's reach around the forearm-to-hand line: thinner than the neck.
inline constexpr float kWristRadiusUnits = 4.0f;

// The wound cap on a wrist (Nadi, 2026-10-31: "go 5"): there is no vanilla wrist
// plug, so the zombie's neck plug serves, on nodes of OBVR's at the hand's
// place and turn - the biped's bones all point along their own x, so the
// plug sits on the wrist as it does on the neck - scaled to a wrist: a neck
// is about 6 units round (neckRadiusUnits), a wrist about 2.7.
inline constexpr float kWristCapScale = 0.45f;

// The plug on a limb's joint, by its girth against a neck's (about 6 units
// round): an elbow about 3.3, a knee about 4.2, a shoulder about 4.8 -
// estimated from the vanilla body, not measured; the eye pictures and the
// headset tell.
inline constexpr float kElbowCapScale = 0.55f;
inline constexpr float kShoulderCapScale = 0.8f;
inline constexpr float kKneeCapScale = 0.7f;

inline float CapScaleFor(CutPart part) {
	switch (part) {
	case CutPart::Head: return 1.0f;
	case CutPart::LeftForearm:
	case CutPart::RightForearm: return kElbowCapScale;
	case CutPart::LeftArm:
	case CutPart::RightArm: return kShoulderCapScale;
	case CutPart::LeftLowerLeg:
	case CutPart::RightLowerLeg: return kKneeCapScale;
	default: return kWristCapScale;
	}
}

// Which way a wound cap faces, from its vertex normals: the vertices and
// normals of its shapes summed in the bone's space (each shape's skin-to-bone
// applied). A normal not of unit length is not trusted for the facing.
struct CapFacing {
	NiPoint3 sum{0.0f, 0.0f, 0.0f};     // of the vertices
	NiPoint3 normal{0.0f, 0.0f, 0.0f};  // of the unit normals
	UInt32 count = 0;
	UInt32 unitNormals = 0;
};

inline void AddCapFacing(CapFacing& f, const NiPoint3* verts, const NiPoint3* normals, UInt32 n,
                         const NiTransform& skinToBone) {
	if (verts == nullptr || normals == nullptr) {
		return;
	}
	for (UInt32 i = 0; i < n; ++i) {
		f.sum = f.sum + skinToBone.pos + skinToBone.rot * (verts[i] * skinToBone.scale);
		++f.count;
		const float len2 = normals[i].LengthSquared();
		if (len2 > 0.81f && len2 < 1.21f) {
			f.normal = f.normal + skinToBone.rot * normals[i];
			++f.unitNormals;
		}
	}
}

inline NiPoint3 CapCentre(const CapFacing& f) {
	return f.count == 0 ? NiPoint3{0.0f, 0.0f, 0.0f} : f.sum * (1.0f / static_cast<float>(f.count));
}

// The share of the cap's mean normal along `dir` (unit): 1 facing it, -1
// facing away, 0 when nothing was read.
inline float CapFacingAlong(const CapFacing& f, const NiPoint3& dir) {
	const float len = std::sqrt(f.normal.LengthSquared());
	if (!(len > 1e-6f)) {
		return 0.0f;
	}
	return (f.normal.x * dir.x + f.normal.y * dir.y + f.normal.z * dir.z) / len;
}

// The bone's transform with the cap turned half round about an axis across
// `down` (the bone's space) through `centre` (the bone's space): the cap stays
// in place and faces the other way. The head's cap faced up into the head,
// its underside seen through (2026-11-03).
inline NiTransform FlippedCap(const NiTransform& bone, const NiPoint3& centre, const NiPoint3& down) {
	const NiPoint3 helper = std::fabs(down.x) < 0.9f ? NiPoint3{1.0f, 0.0f, 0.0f} : NiPoint3{0.0f, 1.0f, 0.0f};
	NiPoint3 a{down.y * helper.z - down.z * helper.y, down.z * helper.x - down.x * helper.z,
	           down.x * helper.y - down.y * helper.x};
	const float len = std::sqrt(a.LengthSquared());
	a = len > 1e-6f ? a * (1.0f / len) : NiPoint3{0.0f, 0.0f, 1.0f};
	NiMatrix33 half{};  // 2 a a^T - I: half a turn about a
	const float v[3] = {a.x, a.y, a.z};
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			half.data[r][c] = 2.0f * v[r] * v[c] - (r == c ? 1.0f : 0.0f);
		}
	}
	NiTransform t = bone;
	t.rot = bone.rot * half;
	t.pos = bone.pos + bone.rot * ((centre - half * centre) * bone.scale);
	return t;
}

// The wrist as a blade meets it: the last stretch of the forearm before the
// hand - from 30 % of the way back toward the elbow (the forearm bone's origin)
// to a tenth past the hand's origin - not the whole forearm.
inline constexpr float kWristBackShare = 0.3f;
inline constexpr float kWristOnShare = 0.1f;

inline void WristSegment(const NiPoint3& forearm, const NiPoint3& hand, NiPoint3& low, NiPoint3& high) {
	low = hand + (forearm - hand) * kWristBackShare;
	high = hand + (hand - forearm) * kWristOnShare;
}

// The reach around a limb's joint a blade has to pass within (units): the
// wrist's for a hand; thicker limbs, wider (estimated like the caps).
inline constexpr float kElbowRadiusUnits = 4.5f;
inline constexpr float kShoulderRadiusUnits = 6.0f;
inline constexpr float kKneeRadiusUnits = 5.5f;

inline float LimbRadiusUnits(CutPart part) {
	switch (part) {
	case CutPart::LeftForearm:
	case CutPart::RightForearm: return kElbowRadiusUnits;
	case CutPart::LeftArm:
	case CutPart::RightArm: return kShoulderRadiusUnits;
	case CutPart::LeftLowerLeg:
	case CutPart::RightLowerLeg: return kKneeRadiusUnits;
	default: return kWristRadiusUnits;
	}
}

// A limb's joint as a blade meets it: around the part bone's origin (`joint`)
// along the part toward its next bone (`next`) - from `back` of the part's
// length up the bone before to `on` of it down the part. The elbow's stretch
// ends before the wrist's begins (WristSegment), the shoulder's is the upper
// arm's top, the knee's the calf's top.
inline void JointSharesOf(CutPart part, float& back, float& on) {
	switch (part) {
	case CutPart::LeftArm:
	case CutPart::RightArm:
		back = 0.05f;
		on = 0.3f;
		return;
	case CutPart::LeftLowerLeg:
	case CutPart::RightLowerLeg:
		back = 0.15f;
		on = 0.2f;
		return;
	default:
		back = 0.15f;
		on = 0.2f;
		return;
	}
}

inline void JointSegment(CutPart part, const NiPoint3& joint, const NiPoint3& next, NiPoint3& low, NiPoint3& high) {
	float back = 0.0f;
	float on = 0.0f;
	JointSharesOf(part, back, on);
	const NiPoint3 along = next - joint;
	low = joint - along * back;
	high = joint + along * on;
}

struct CutMemory {
	UInt32 ids[kCutMemoryMost] = {};
	UInt8 parts[kCutMemoryMost] = {};
	UInt32 count = 0;
};

inline bool CutRemembered(const CutMemory& m, UInt32 id, CutPart part = CutPart::Head) {
	for (UInt32 i = 0; i < m.count && i < kCutMemoryMost; ++i) {
		if (m.ids[i] == id && m.parts[i] == static_cast<UInt8>(part)) {
			return true;
		}
	}
	return false;
}

inline bool RememberCut(CutMemory& m, UInt32 id, CutPart part = CutPart::Head) {
	if (id == 0 || static_cast<UInt32>(part) >= kCutPartCount || CutRemembered(m, id, part)) {
		return false;
	}
	if (m.count >= kCutMemoryMost) {
		for (UInt32 i = 1; i < kCutMemoryMost; ++i) {
			m.ids[i - 1] = m.ids[i];
			m.parts[i - 1] = m.parts[i];
		}
		m.count = kCutMemoryMost - 1;
	}
	m.ids[m.count] = id;
	m.parts[m.count] = static_cast<UInt8>(part);
	++m.count;
	return true;
}

inline bool RedoDue(float secondsWithThreeD) { return secondsWithThreeD >= kRedoAfterSeconds; }

// Whether a cut made in world `cutWorld` (game/RefFingerprint.h
// CurrentWorld, a new one on every frame a load runs) may be read in
// `worldNow`. 0 is no world.
inline bool CutInWorld(UInt32 cutWorld, UInt32 worldNow) { return cutWorld != 0 && cutWorld == worldNow; }

// A cut keeps raw pointers to its actor and that actor's 3D. A load may free
// the actor - a reference made at runtime (the crash of 2026-11-07 23:18:
// StepCuts read a freed walker at +0x3C in the load's first frames) - or
// keep it and its 3D as they were (a load in the same cell, 2026-10-31
// 19:47, where the skins still point at the cut's own nodes). So after a
// load the cut's actor is not read until the engine's own form table, asked
// for the cut's form id, still answers with that pointer:
// - Read: made in this world;
// - Recheck: a load since, and the table holds the same actor - the cut is
//   taken into this world and read from the next frame on (a frame of a
//   load moves the world on again, so it is read only once the load is
//   done); its 3D checks (root, skins) then decide as before;
// - Drop: a load since, and the table answers otherwise or not at all - the
//   actor is gone, the cut dropped unread; its caps went with the old 3D and
//   the co-save's memory cuts the part again once the actor's 3D is back.
enum class CutWorldStep { Read, Recheck, Drop };

inline CutWorldStep CutWorldFor(UInt32 cutWorld, UInt32 worldNow, UInt32 cutActor, UInt32 actorByFormId) {
	if (CutInWorld(cutWorld, worldNow)) {
		return CutWorldStep::Read;
	}
	return cutActor != 0 && actorByFormId == cutActor ? CutWorldStep::Recheck : CutWorldStep::Drop;
}

// A remembered cut whose actor's 3D is there again: cut again once it has
// stood (a load, the cell again) while the actor is dead; forgotten when it
// is alive - raised again (Walkers' pool walkers, a Resurrect; the engine
// gives it its whole 3D back, measured 2026-11-05 in dismember-resurrect), so
// the cut went with its death; waited on while it is not among the loaded
// actors (its state unread).
enum class RedoStep { Wait, Redo, Forget };

inline RedoStep RedoFor(bool loaded, bool dead, float secondsWithThreeD) {
	if (!loaded) {
		return RedoStep::Wait;
	}
	if (!dead) {
		return RedoStep::Forget;
	}
	return RedoDue(secondsWithThreeD) ? RedoStep::Redo : RedoStep::Wait;
}

// Every remembered part of `id` forgotten; answers how many.
inline UInt32 ForgetCuts(CutMemory& m, UInt32 id) {
	UInt32 kept = 0;
	UInt32 gone = 0;
	for (UInt32 i = 0; i < m.count && i < kCutMemoryMost; ++i) {
		if (m.ids[i] == id) {
			++gone;
			continue;
		}
		m.ids[kept] = m.ids[i];
		m.parts[kept] = m.parts[i];
		++kept;
	}
	m.count = kept;
	return gone;
}

// 'OBCH' version 3: the ids and each one's part, the limbs among them
// (2026-11-05). Version 2 (the hands, of 2026-10-31) is the same layout and
// knows only the head and the hands; version 1 (heads only, of 2026-10-31
// 19:51) is read as heads. A DLL of version 2 refuses a version 3 record
// whole rather than read a limb as a head.
inline constexpr UInt32 kCutHeadsRecord = 0x4843424F;  // 'OBCH'
inline constexpr UInt32 kCutHeadsVersion = 3;
inline constexpr UInt32 kCutHeadsVersionHands = 2;
inline constexpr UInt32 kCutHeadsVersionHeadsOnly = 1;

struct CutHeadsRecord {
	UInt32 count = 0;
	UInt32 ids[kCutMemoryMost] = {};
	UInt8 parts[kCutMemoryMost] = {};
};
inline constexpr UInt32 kCutHeadsRecordV1Bytes = 4 + 4 * kCutMemoryMost;

inline CutHeadsRecord RecordOfCuts(const CutMemory& m) {
	CutHeadsRecord r;
	r.count = m.count < kCutMemoryMost ? m.count : kCutMemoryMost;
	for (UInt32 i = 0; i < r.count; ++i) {
		r.ids[i] = m.ids[i];
		r.parts[i] = m.parts[i];
	}
	return r;
}

// The record read back: each id through `resolve` (xOBSE's ResolveRefID: the
// load order may have moved it, a created reference may be gone). False, and
// nothing kept, for a record of another length or version.
template <typename Resolve>
inline bool CutsOfRecord(const CutHeadsRecord* r, UInt32 length, UInt32 version, Resolve resolve, CutMemory& out) {
	out = CutMemory{};
	const bool full = length == sizeof(CutHeadsRecord);
	const bool v3 = version == kCutHeadsVersion && full;
	const bool v2 = version == kCutHeadsVersionHands && full;
	const bool v1 = version == kCutHeadsVersionHeadsOnly && length == kCutHeadsRecordV1Bytes;
	if (r == nullptr || (!v1 && !v2 && !v3) || r->count > kCutMemoryMost) {
		return false;
	}
	const UInt32 known = v3 ? kCutPartCount : kCutPartCountHands;
	for (UInt32 i = 0; i < r->count; ++i) {
		UInt32 id = 0;
		if (!v1 && r->parts[i] >= known) {
			continue;  // a part this version does not know: not read as a head
		}
		const CutPart part = v1 ? CutPart::Head : static_cast<CutPart>(r->parts[i]);
		if (resolve(r->ids[i], id)) {
			RememberCut(out, id, part);
		}
	}
	return true;
}

}  // namespace obvr::game
