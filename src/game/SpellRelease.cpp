#include "game/SpellRelease.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/GameSound.h"
#include "game/SpellOrb.h"

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
	UInt32 slotSeen[6] = {};  // per animation slot: group code << 8 | key counter, as last logged
	UInt32 animLines = 80;
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
