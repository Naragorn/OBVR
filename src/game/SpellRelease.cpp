#include "game/SpellRelease.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GameSound.h"

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
	UInt32 cueLines = 12;
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

void Release(int site, void* caster, void* edx, UInt32 arg) {
	Probe& p = State();
	const bool player = caster == PlayerCaster();
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
}

void StepSpellReleaseProbe(bool castHeld) {
	Probe& p = State();
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
}

bool SpellChargedInHand() {
	const Probe& p = State();
	return p.heldCaster != nullptr || p.fire;
}

void SetSpellReleaseHold(bool hold) {
	Probe& p = State();
	p.hold = hold;
	p.holdTick = p.tick;
}

void RequestHeldSpellRelease(int stage) {
	Probe& p = State();
	p.fire = true;
	p.fireStage = stage;
}

void DropHeldSpellRelease() {
	Probe& p = State();
	if (p.heldCaster != nullptr && p.lines > 0) {
		--p.lines;
		OBVR_LOG("Spell release: the held spell DROPPED (a menu came up) - no spell, no magicka");
	}
	p.heldCaster = nullptr;
	p.heldItem = nullptr;
	p.fire = false;
}

void StepHeldSpellRelease() {
	Probe& p = State();
	if (!p.fire) {
		return;
	}
	p.fire = false;
	void* const caster = p.heldCaster;
	p.heldCaster = nullptr;
	if (caster == nullptr) {
		// Let go before the engine came to its release: it makes the spell itself, when it comes.
		if (p.lines > 0) {
			--p.lines;
			OBVR_LOG("Spell release: let go at stage %d before the engine's release (frame %u) - it releases it itself",
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
	const float magickaBefore = PlayerMagicka();
	reinterpret_cast<ReleaseFn>(p.next[p.heldSite])(caster, nullptr, 0);
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

namespace {

// The active spell's first effect's casting sound: the item's effect list (MagicItem +0x0C, its head
// effect at +0x10), the EffectItem's setting (+0x1C), the setting's casting sound (+0x80), the
// sound's form id (+0x0C) - xOBSE GameForms.h's offsets. 0 when any link is missing.
UInt32 CastingSoundOf(const void* item) {
	if (item == nullptr) {
		return 0;
	}
	const UInt8* const effect = *reinterpret_cast<const UInt8* const*>(static_cast<const UInt8*>(item) + 0x10);
	const UInt8* const setting = effect != nullptr ? *reinterpret_cast<const UInt8* const*>(effect + 0x1C) : nullptr;
	const UInt8* const sound = setting != nullptr ? *reinterpret_cast<const UInt8* const*>(setting + 0x80) : nullptr;
	return sound != nullptr ? *reinterpret_cast<const UInt32*>(sound + 0x0C) : 0;
}

}  // namespace

void SpellChargeCue(int stage) {
	Probe& p = State();
	void* const caster = PlayerCaster();
	void* const item = p.heldItem != nullptr ? p.heldItem : (caster != nullptr ? ActiveItem(caster) : nullptr);
	const UInt32 sound = CastingSoundOf(item);
	const float volume = stage >= 3 ? 1.0f : stage == 2 ? 0.7f : 0.4f;
	const bool played = sound != 0 && PlayLoadedSoundVolume(sound, volume);
	if (p.cueLines > 0) {
		--p.cueLines;
		OBVR_LOG("Spell charge: stage %d - the spell's casting sound %08X %s at %.1f", stage, sound,
		         played ? "played" : "NOT played", static_cast<double>(volume));
	}
}

}  // namespace obvr::game
