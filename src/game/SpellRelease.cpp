#include "game/SpellRelease.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/FirstPersonHide.h"
#include "game/GameSound.h"
#include "game/OwnedNode.h"
#include "game/PlayerBody.h"
#include "game/SpellOrb.h"
#include "game/SpellStrength.h"
#include "vr/SpellCharge.h"

namespace obvr::game {
namespace {

constexpr UInt32 kRelease = 0x0069BEC0;
constexpr UInt32 kSites[2] = {0x005FCE1D, 0x005FCF6E};
// [caster+8], the MagicCaster's state: 3 after a release that made the spell,
// 7 after one that could not be paid for (0x0069BEC0); 5 and 6 refusals at the
// cast's start (0x00699190).
constexpr UInt32 kCasterStateOffset = 0x08;

using ReleaseFn = void(__fastcall*)(void* caster, void* edx, UInt32 arg);
// The caster's active item: get (virtual +0x30, [caster+0x18C]) and set (virtual +0x34), as
// 0x0069BE00 sets it before it applies one; the release reads it (0x0069BEC0's first call). A
// second item word beside it (+0x38 get / +0x3C set, [caster+0x190]) is set with it by
// CastMagicItem 0x00699190 and cleared with it by a release that could not be paid for.
using GetItemFn = void*(__fastcall*)(void* caster, void* edx);
using SetItemFn = void(__fastcall*)(void* caster, void* edx, void* item);

void* ItemAt(void* caster, UInt32 getSlot) {
	const UInt32* const table = *reinterpret_cast<UInt32* const*>(caster);
	return reinterpret_cast<GetItemFn>(table[getSlot / 4])(caster, nullptr);
}

void SetItemAt(void* caster, UInt32 setSlot, void* item) {
	const UInt32* const table = *reinterpret_cast<UInt32* const*>(caster);
	reinterpret_cast<SetItemFn>(table[setSlot / 4])(caster, nullptr, item);
}

void* ActiveItem(void* caster) { return ItemAt(caster, 0x30); }

// The player's magicka now (actor value 9 through the actor's virtual +0x288, as HintReads.cpp
// reads one): the log's proof that a release took its cost. -1 when unreadable.
float PlayerMagicka() {
	UInt8* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (player == nullptr) {
		return -1.0f;
	}
	const UInt32 slot = (*reinterpret_cast<const UInt32* const*>(player))[0x288 / 4];
	using GetFloatFn = float(__fastcall*)(void* actor, void* edx, UInt32 av);
	return reinterpret_cast<GetFloatFn>(slot)(player, nullptr, 9);
}

// What the probe keeps between frames, in one place.
struct Probe {
	UInt32 next[2] = {0, 0};  // what each site called before OBVR took it
	bool installed[2] = {false, false};
	UInt32 lines = 60;
	UInt32 frame = 0;         // frames since the cast control was pressed
	bool castWas = false;
	SInt32 stateWas = -1;
	bool watching = false;
	UInt32 quietFrames = 0;
	bool hold = false;        // the player's release held back now
	UInt32 holdTick = 0;      // the tick it was last said: a hold not said again lapses
	UInt32 tick = 0;          // frames, counting on
	void* heldCaster = nullptr;  // a held release, waiting
	void* heldItem = nullptr;    // the caster's active item when it was held back
	void* heldSecond = nullptr;  // and the item word beside it
	UInt32 heldSite = 0;
	UInt32 heldFrame = 0;
	bool fire = false;        // make it at the next animation update
	int fireStage = 0;
	float fireFactor = 1.0f;  // the stage's strength and cost wanted (vr::SpellChargeFactor)
	UInt32 slotSeen[6] = {};  // per animation slot: group code << 8 | key counter, as last logged
	UInt32 animLines = 80;
	UInt32 cueLines = 12;
	vr::CastAnimPause pause[2];  // the cast's animation kept still: first-, third-person
	UInt32 pauseLines = 24;
	float clockWas = 0.0f;       // the first-person anim data's clock last frame
	float effectStill = 0.0f;    // seconds the engine's hand effect was kept still
	UInt32 effectMoved = 0;      // its sequences moved last frame
	NiAVObject* effectNodes[2] = {nullptr, nullptr};  // the magicNodes it hangs on: first-, third-person
	UInt32 effectLines = 16;
	bool castHeld = false;       // the trigger held since its press (SetSpellCastHeld)
	UInt32 castHeldTick = 0;
	bool pending = false;        // let go before the engine's release: that release gets the stage
	UInt32 pendingTick = 0;
	int pendingStage = 0;
	float pendingFactor = 1.0f;
	bool swallow = false;        // dropped before the engine's release: that release makes nothing
	UInt32 swallowTick = 0;
};

Probe& State() {
	static Probe probe;
	return probe;
}

void* PlayerCaster() {
	UInt8* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	return player == nullptr ? nullptr : player + addr::kPlayerMagicCasterOffset;
}

SInt32 CasterState(void* caster) {
	return caster == nullptr ? -1 : *reinterpret_cast<const SInt32*>(static_cast<UInt8*>(caster) + kCasterStateOffset);
}

// A dropped charge's release still to come is swallowed within this many frames (10 s at 72 Hz);
// later, or after the next press, it is the game's again.
constexpr UInt32 kSwallowTicks = 720;

// A release made at a charge's stage: its strength as far as the magicka pays for it
// (vr/SpellCharge.h), the effects made at that while the release is made (game/SpellStrength.h), the
// rest of the cost taken after it. Answers the magicka before it.
float MakeChargedRelease(Probe& p, int site, void* caster, void* edx, UInt32 arg, void* item, int stage,
                         float wanted) {
	const float magickaBefore = PlayerMagicka();
	const float cost = PlayerSpellCost(item);
	const float factor = vr::AffordableChargeFactor(wanted, magickaBefore, cost);
	ArmChargedRelease(factor);
	reinterpret_cast<ReleaseFn>(p.next[site])(caster, edx, arg);
	DisarmChargedRelease();
	const float paid = magickaBefore - PlayerMagicka();
	const float extra = vr::ExtraChargeCost(paid, factor);
	TakePlayerMagicka(extra);
	if (p.lines > 0 && wanted != 1.0f) {
		--p.lines;
		OBVR_LOG("Spell strength: stage %d wants %.2f; the spell costs %.1f, magicka %.1f - made at %.2f; the engine "
		         "took %.1f, %.1f more taken",
		         stage, static_cast<double>(wanted), static_cast<double>(cost), static_cast<double>(magickaBefore),
		         static_cast<double>(factor), static_cast<double>(paid), static_cast<double>(extra));
	}
	return magickaBefore;
}

void Release(int site, void* caster, void* edx, UInt32 arg) {
	Probe& p = State();
	const bool player = caster == PlayerCaster();
	if (player && p.swallow) {
		p.swallow = false;
		if (p.tick <= p.swallowTick + kSwallowTicks) {
			if (p.lines > 0) {
				--p.lines;
				OBVR_LOG("Spell release: at %08X for the player - the charge was dropped before it: SWALLOWED - the "
				         "dropped charge made no spell, no magicka",
				         kSites[site]);
			}
			return;
		}
	}
	if (player && p.pending) {
		p.pending = false;
		MakeChargedRelease(p, site, caster, edx, arg, ActiveItem(caster), p.pendingStage, p.pendingFactor);
		if (p.lines > 0) {
			--p.lines;
			OBVR_LOG("Spell release: the engine's own release after the let go made at stage %d, frame %u since the "
			         "press",
			         p.pendingStage, p.frame);
		}
		return;
	}
	if (player && p.hold && p.tick <= p.holdTick + 2 && p.heldCaster == nullptr) {
		p.heldCaster = caster;
		p.heldItem = ActiveItem(caster);
		p.heldSecond = ItemAt(caster, 0x38);
		p.heldSite = static_cast<UInt32>(site);
		p.heldFrame = p.frame;
		if (p.lines > 0) {
			--p.lines;
			OBVR_LOG("Spell release: at %08X for the player - HELD BACK, the spell charged in the hand, frame %u since "
			         "the press",
			         kSites[site], p.frame);
		}
		return;
	}
	const float magickaBefore = player ? PlayerMagicka() : -1.0f;
	reinterpret_cast<ReleaseFn>(p.next[site])(caster, edx, arg);
	if (p.lines > 0) {
		--p.lines;
		OBVR_LOG("Spell release: at %08X for %s - frame %u since the press; magicka %.1f before, %.1f after",
		         kSites[site], player ? "the player" : "another caster", p.frame, static_cast<double>(magickaBefore),
		         static_cast<double>(player ? PlayerMagicka() : -1.0f));
	}
}

}  // namespace

void __fastcall OBVR_SpellReleaseA(void* caster, void* edx, UInt32 arg) { Release(0, caster, edx, arg); }
void __fastcall OBVR_SpellReleaseB(void* caster, void* edx, UInt32 arg) { Release(1, caster, edx, arg); }

void InstallSpellRelease() {
	Probe& p = State();
	const UInt32 hooks[2] = {reinterpret_cast<UInt32>(&OBVR_SpellReleaseA), reinterpret_cast<UInt32>(&OBVR_SpellReleaseB)};
	for (int i = 0; i < 2; ++i) {
		const UInt8* const site = reinterpret_cast<const UInt8*>(kSites[i]);
		if (site[0] != 0xE8) {
			continue;
		}
		const UInt32 next = kSites[i] + 5 + *reinterpret_cast<const UInt32*>(site + 1);
		const UInt32 to = hooks[i] - (kSites[i] + 5);
		p.next[i] = next;
		p.installed[i] = next != 0 && mem::SafeWrite(kSites[i] + 1, &to, sizeof(to));
		OBVR_LOG("Spell release: the call at %08X %s (it called %08X; the release is %08X)", kSites[i],
		         p.installed[i] ? "watched" : "NOT watched", next, kRelease);
	}
	InstallSpellStrength();
}

// The player's first-person animation slots through a cast (the probe for holding the cast's own
// animation, as game/BowRelease.cpp holds the bow's draw): per slot its group's code and its key
// counter, logged as they change, with the sequence's time (its offset +0x48 plus the anim data's
// clock +0x94) - the time a counter steps is its key's time.
namespace {

// The active spell's first effect's setting: the item's effect list (MagicItem +0x0C, its head
// effect at +0x10), the EffectItem's setting (+0x1C) - xOBSE GameForms.h's offsets. nullptr when a
// link is missing.
const UInt8* FirstEffectSetting(const void* item) {
	if (item == nullptr) {
		return nullptr;
	}
	const UInt8* const effect = *reinterpret_cast<const UInt8* const*>(static_cast<const UInt8*>(item) + 0x10);
	return effect != nullptr ? *reinterpret_cast<const UInt8* const*>(effect + 0x1C) : nullptr;
}

// Its casting sound (+0x80) and that sound's form id (+0x0C); 0 when missing.
UInt32 CastingSoundOf(const void* item) {
	const UInt8* const setting = FirstEffectSetting(item);
	const UInt8* const sound = setting != nullptr ? *reinterpret_cast<const UInt8* const*>(setting + 0x80) : nullptr;
	return sound != nullptr ? *reinterpret_cast<const UInt32*>(sound + 0x0C) : 0;
}

// Its model (EffectSetting's TESModel at +0x18, the path's characters at +0x1C): the projectile the
// release flies (Oblivion.esm, read 2026-11-23: FIDG MagicEffects\Fireball.NIF).
const char* ModelPathOf(const void* item) {
	const UInt8* const setting = FirstEffectSetting(item);
	const char* const path = setting != nullptr ? *reinterpret_cast<const char* const*>(setting + 0x1C) : nullptr;
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(path)) ? path : nullptr;
}

// The spell being charged: the held item, or the caster's own before its release was held back.
void* ChargedItem(const Probe& p) {
	void* const caster = PlayerCaster();
	return p.heldItem != nullptr ? p.heldItem : (caster != nullptr ? ActiveItem(caster) : nullptr);
}

void LogCastAnimation(Probe& p) {
	const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	const UInt32 animData = player != 0 ? *reinterpret_cast<const UInt32*>(player + 0x5CC) : 0;
	if (!mem::LooksLikeObjectAddress(animData)) {
		return;
	}
	const float clock = *reinterpret_cast<const float*>(animData + 0x94);
	for (UInt32 slot = 0; slot < 6; ++slot) {
		const UInt32 sequence = *reinterpret_cast<const UInt32*>(animData + 0xA0 + slot * 4);
		const UInt32 group = mem::LooksLikeObjectAddress(sequence) ? *reinterpret_cast<const UInt32*>(sequence + 0x68) : 0;
		const UInt32 code = mem::LooksLikeObjectAddress(group) ? *reinterpret_cast<const UInt8*>(group + 0x08) : 0xFF;
		const UInt32 counter = *reinterpret_cast<const UInt32*>(animData + 0x48 + slot * 4) & 0xFF;
		const UInt32 seen = (code << 8) | counter;
		if (seen == p.slotSeen[slot]) {
			continue;
		}
		p.slotSeen[slot] = seen;
		if (p.animLines > 0 && code != 0xFF) {
			--p.animLines;
			const float time = clock + *reinterpret_cast<const float*>(sequence + 0x48);
			OBVR_LOG("Spell release: frame %u - animation slot %u group 0x%02X key %u at %.3f s", p.frame, slot, code,
			         counter, static_cast<double>(time));
		}
	}
}

// The cast's animation paused at its release while the spell is held (vr::StepCastAnimPause), on the
// first-person anim data (player +0x5CC) and the third-person one (the process at player +0x58, its
// +0x17C) - the anim data, its slots and its clock as game/BowRelease.h reads them for the bow's draw.
UInt32 AnimDataOf(int which) {
	const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	if (player == 0) {
		return 0;
	}
	if (which == 0) {
		return *reinterpret_cast<const UInt32*>(player + 0x5CC);
	}
	const UInt32 process = *reinterpret_cast<const UInt32*>(player + 0x58);
	return mem::LooksLikeObjectAddress(process) ? *reinterpret_cast<const UInt32*>(process + 0x17C) : 0;
}

// The first slot playing a cast group, its sequence (0 when none) and the group's code.
UInt32 CastSequenceOf(UInt32 animData, unsigned& code) {
	code = 0xFF;
	if (!mem::LooksLikeObjectAddress(animData)) {
		return 0;
	}
	for (UInt32 slot = 0; slot < 6; ++slot) {
		const UInt32 sequence = *reinterpret_cast<const UInt32*>(animData + 0xA0 + slot * 4);
		const UInt32 group = mem::LooksLikeObjectAddress(sequence) ? *reinterpret_cast<const UInt32*>(sequence + 0x68) : 0;
		const unsigned c = mem::LooksLikeObjectAddress(group) ? *reinterpret_cast<const UInt8*>(group + 0x08) : 0xFF;
		if (vr::IsCastAnimGroup(c)) {
			code = c;
			return sequence;
		}
	}
	return 0;
}

void PauseCastAnimation(Probe& p, bool heldBack, bool castHeld) {
	for (int i = 0; i < 2; ++i) {
		const UInt32 animData = AnimDataOf(i);
		unsigned code = 0xFF;
		const UInt32 sequence = CastSequenceOf(animData, code);
		const float clock = sequence != 0 ? *reinterpret_cast<const float*>(animData + 0x94) : 0.0f;
		float* const offset = sequence != 0 ? reinterpret_cast<float*>(sequence + 0x48) : nullptr;
		const bool was = p.pause[i].paused;
		float want = 0.0f;
		if (vr::StepCastAnimPause(p.pause[i], heldBack && offset != nullptr, castHeld && offset != nullptr, code,
		                          offset != nullptr ? *offset + clock : 0.0f, clock, want)) {
			*offset = want;
		}
		if (was != p.pause[i].paused && p.pauseLines > 0) {
			--p.pauseLines;
			const char* const which = i == 0 ? "first-person" : "third-person";
			if (p.pause[i].paused) {
				OBVR_LOG("Spell cast animation: the %s cast (group 0x%02X) paused %s, %.3f s - %s waits for the let go",
				         which, code, heldBack ? "at its release" : "at its start", static_cast<double>(p.pause[i].time),
				         heldBack ? "its second half" : "all of it");
			} else {
				OBVR_LOG("Spell cast animation: the %s cast plays on (%s), frame %u since the press", which,
				         heldBack || castHeld ? "the slot no longer a cast" : "the spell let go or dropped", p.frame);
			}
		}
	}
}

// The engine's own hand effect kept still with the cast (Nadi, 2026-11-24: "der helle stern beim
// selbst zauber zurückhalten"): the spell model "BASE <model>" the engine hangs on the caster's
// magicNode with its "SpecialIdle_Cast" (0x005EDCE0, from 0x005F4190 while the cast's sequence
// animates; probe 2026-11-24: on the first-person magicNode, beside a NiPointLight, the casting light).
// While the cast's animation waits, the effect's sequence keeps the time it had - its offset moved on
// by the anim clock's step each frame - and it plays on from there with the cast. The heal's star, the
// model's burst 0.65-1.05 s into it, so comes after the let go, as it comes after the release in
// vanilla.
// The skeletons' "magicNode"s, first- and third-person, looked up once a pause begins. Not the caster's
// own answer (its virtual +0x24): it picks the skeleton by the third-person root's hidden bit, which
// the body's draw changes within a frame - in the animation update it answered the empty
// third-person node (harness 2026-11-24: "0 children" there, the model on the first-person one).
void FindMagicNodes(Probe& p) {
	p.effectNodes[0] = FindFirstPersonNode("magicNode");
	p.effectNodes[1] = nullptr;
	NiAVObject* const body = PlayerBodyRoot();
	if (body != nullptr) {
		const char* const names[1] = {"magicNode"};
		FindNodesNamed(body, names, 1, &p.effectNodes[1]);
	}
}

// Moves each running "SpecialIdle_Cast" under `node` on by nothing: its offset back by the step.
UInt32 KeepStillUnder(NiAVObject* node, float step, const char*& first) {
	UInt32 count = 0;
	NiAVObject* const* const children = node != nullptr ? NodeChildren(node, count) : nullptr;
	UInt32 moved = 0;
	for (UInt32 i = 0; children != nullptr && i < count; ++i) {
		const char* why = nullptr;
		UInt8* const sequence = children[i] != nullptr ? NodeSequence(children[i], "SpecialIdle_Cast", why) : nullptr;
		if (sequence == nullptr || *reinterpret_cast<const UInt32*>(sequence + 0x44) == 0) {
			continue;
		}
		float& offset = *reinterpret_cast<float*>(sequence + 0x48);
		if (offset > -1e30f) {
			offset = vr::StillSequenceOffset(offset, step);
			++moved;
			first = first != nullptr ? first : children[i]->name;
		}
	}
	return moved;
}

void KeepHandEffectStill(Probe& p, bool still) {
	const UInt32 animData = AnimDataOf(0);
	const float clock = mem::LooksLikeObjectAddress(animData) ? *reinterpret_cast<const float*>(animData + 0x94) : 0.0f;
	const float step = vr::AnimClockStep(clock, p.clockWas);
	p.clockWas = clock;
	if (!still) {
		if (p.effectStill > 0.0f && p.effectLines > 0) {
			--p.effectLines;
			OBVR_LOG("Spell hand effect: plays on with the cast - kept still %.2f s, %u sequence(s), frame %u since the "
			         "press",
			         static_cast<double>(p.effectStill), p.effectMoved, p.frame);
		}
		p.effectStill = 0.0f;
		p.effectMoved = 0;
		p.effectNodes[0] = p.effectNodes[1] = nullptr;
		return;
	}
	if (p.effectStill == 0.0f) {
		FindMagicNodes(p);
	}
	const char* first = nullptr;
	const UInt32 moved = KeepStillUnder(p.effectNodes[0], step, first) + KeepStillUnder(p.effectNodes[1], step, first);
	if (p.effectStill == 0.0f && p.effectLines > 0) {
		--p.effectLines;
		OBVR_LOG("Spell hand effect: kept still with the cast - %u sequence(s) (%s) on the magicNodes %08X/%08X, frame "
		         "%u since the press",
		         moved, first != nullptr ? first : "none running", reinterpret_cast<UInt32>(p.effectNodes[0]),
		         reinterpret_cast<UInt32>(p.effectNodes[1]), p.frame);
	}
	p.effectStill += step > 0.0f ? step : 1e-6f;
	p.effectMoved = moved > p.effectMoved ? moved : p.effectMoved;
}

}  // namespace

void StepSpellReleaseProbe(bool castHeld) {
	Probe& p = State();
	if (p.watching) {
		LogCastAnimation(p);
	}
	void* const caster = PlayerCaster();
	const SInt32 state = CasterState(caster);
	if (castHeld && !p.castWas) {
		p.frame = 0;
		p.watching = true;
		p.quietFrames = 0;
	}
	if (p.watching && p.lines > 0 && (state != p.stateWas || castHeld != p.castWas)) {
		--p.lines;
		OBVR_LOG("Spell release: frame %u since the press - the cast control %s, the caster's state %d", p.frame,
		         castHeld ? "held" : "up", static_cast<int>(state));
	}
	p.quietFrames = state == p.stateWas && !castHeld ? p.quietFrames + 1 : 0;
	if (p.quietFrames > 300) {
		p.watching = false;
	}
	p.stateWas = state;
	p.castWas = castHeld;
	++p.frame;
	++p.tick;
	StepSpellOrb(ModelPathOf(ChargedItem(p)));
}

bool SpellChargedInHand() {
	const Probe& p = State();
	return p.heldCaster != nullptr || p.fire || p.pending || p.pause[0].paused;
}

bool SpellReleaseAwaited() {
	const Probe& p = State();
	return p.fire || p.pending;
}

void SetSpellReleaseHold(bool hold) {
	Probe& p = State();
	p.hold = hold;
	p.holdTick = p.tick;
}

void RequestHeldSpellRelease(int stage, float factor) {
	Probe& p = State();
	p.fire = true;
	p.fireStage = stage;
	p.fireFactor = factor;
}

void DropHeldSpellRelease() {
	Probe& p = State();
	if (p.heldCaster != nullptr && p.lines > 0) {
		--p.lines;
		OBVR_LOG("Spell release: the held spell DROPPED (a menu came up) - no spell, no magicka");
	}
	if (p.heldCaster == nullptr) {
		// Dropped before the engine came to its release (a touch or target spell always): that release,
		// when the cast plays on, makes nothing.
		p.swallow = true;
		p.swallowTick = p.tick;
		if (p.lines > 0) {
			--p.lines;
			OBVR_LOG("Spell release: the charge DROPPED before the engine's release (a menu came up) - its release "
			         "will make nothing");
		}
	}
	p.heldCaster = nullptr;
	p.heldItem = nullptr;
	p.fire = false;
}

void StepHeldSpellRelease() {
	Probe& p = State();
	const bool castHeld = p.castHeld && p.tick <= p.castHeldTick + 2;
	PauseCastAnimation(p, p.heldCaster != nullptr && !p.fire, castHeld);
	KeepHandEffectStill(p, p.pause[0].paused || p.pause[1].paused);
	unsigned code = 0xFF;
	const bool castOn = CastSequenceOf(AnimDataOf(0), code) != 0;
	if (p.swallow && !castOn) {
		// The dropped cast ended without coming to its release (a menu ends it so, measured
		// 2026-11-24): nothing left to swallow.
		p.swallow = false;
		if (p.lines > 0) {
			--p.lines;
			OBVR_LOG("Spell release: the dropped cast ended without its release - the dropped charge made no spell, no "
			         "magicka");
		}
	}
	if (p.pending && (!castOn || p.tick > p.pendingTick + kSwallowTicks)) {
		// Let go, and the cast ended (or 10 s went) without the engine's release: no spell comes, and
		// nothing stays in the hand (SpellReleaseAwaited).
		p.pending = false;
		if (p.lines > 0) {
			--p.lines;
			OBVR_LOG("Spell release: the cast ended without its release after the let go - no spell");
		}
	}
	if (!p.fire) {
		return;
	}
	p.fire = false;
	void* const caster = p.heldCaster;
	p.heldCaster = nullptr;
	if (caster == nullptr) {
		// Let go before the engine came to its release (a touch or target spell always): its own
		// release, when it comes, is made at the stage.
		p.pending = true;
		p.pendingTick = p.tick;
		p.pendingStage = p.fireStage;
		p.pendingFactor = p.fireFactor;
		if (p.lines > 0) {
			--p.lines;
			OBVR_LOG("Spell release: let go at stage %d before the engine's release (frame %u) - its own release gets "
			         "the stage",
			         p.fireStage, p.frame);
		}
		return;
	}
	if (caster != PlayerCaster()) {
		return;  // a load in between: not the player's caster any more
	}
	// The cast's animation may have ended meanwhile and its item gone: given back first.
	void* const itemNow = ActiveItem(caster);
	if (itemNow != p.heldItem && p.heldItem != nullptr) {
		SetItemAt(caster, 0x34, p.heldItem);
		SetItemAt(caster, 0x3C, p.heldSecond);
	}
	const float magickaBefore =
	    MakeChargedRelease(p, static_cast<int>(p.heldSite), caster, nullptr, 0, p.heldItem, p.fireStage, p.fireFactor);
	if (p.lines > 0) {
		--p.lines;
		OBVR_LOG("Spell release: the held spell MADE at stage %d - held back at frame %u, made at frame %u since the "
		         "press; the caster's item %08X when held, %08X at the release%s; magicka %.1f before, %.1f after",
		         p.fireStage, p.heldFrame, p.frame, reinterpret_cast<UInt32>(p.heldItem), reinterpret_cast<UInt32>(itemNow),
		         itemNow != p.heldItem ? " - given back" : "", static_cast<double>(magickaBefore),
		         static_cast<double>(PlayerMagicka()));
	}
	p.heldItem = nullptr;
}

void SetSpellCastHeld(bool held) {
	Probe& p = State();
	if (held && !p.castHeld) {
		// A new press: what an earlier one left for the engine's release is over.
		p.pending = false;
		p.swallow = false;
	}
	p.castHeld = held;
	p.castHeldTick = p.tick;
}

void SpellChargeCue(int stage) {
	Probe& p = State();
	const UInt32 sound = CastingSoundOf(ChargedItem(p));
	const float volume = stage >= 3 ? 1.0f : stage == 2 ? 0.7f : 0.4f;
	const bool played = sound != 0 && PlayLoadedSoundVolume(sound, volume);
	if (p.cueLines > 0) {
		--p.cueLines;
		OBVR_LOG("Spell charge: stage %d - the spell's casting sound %08X %s at %.1f", stage, sound,
		         played ? "played" : "NOT played", static_cast<double>(volume));
	}
}

}  // namespace obvr::game
