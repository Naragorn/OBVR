#include "game/SpellRelease.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kRelease = 0x0069BEC0;
constexpr UInt32 kSites[2] = {0x005FCE1D, 0x005FCF6E};
// [caster+8], the MagicCaster's state: 3 after a release that made the spell,
// 7 after one that could not be paid for (0x0069BEC0); 5 and 6 refusals at the
// cast's start (0x00699190).
constexpr UInt32 kCasterStateOffset = 0x08;

using ReleaseFn = void(__fastcall*)(void* caster, void* edx, UInt32 arg);

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
	const SInt32 before = CasterState(caster);
	reinterpret_cast<ReleaseFn>(p.next[site])(caster, edx, arg);
	if (p.lines > 0) {
		--p.lines;
		OBVR_LOG("Spell release: at %08X for %s - the caster's state %d before, %d after, frame %u since the press",
		         kSites[site], player ? "the player" : "another caster", static_cast<int>(before),
		         static_cast<int>(CasterState(caster)), p.frame);
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
}

}  // namespace obvr::game
