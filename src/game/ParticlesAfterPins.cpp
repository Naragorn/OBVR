#include "game/ParticlesAfterPins.h"

#include <intrin.h>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/FirstPersonArms.h"
#include "game/GameTypes.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// NiPSysUpdateCtlr::Update(float time), __thiscall with `ret 4`: the same
// call as __fastcall with a dead edx.
using UpdateFn = void(__fastcall*)(void* controller, void* unusedEdx, float time);

UpdateFn g_original = nullptr;         // NiPSysUpdateCtlr::Update
UpdateFn g_originalEmitter = nullptr;  // NiPSysEmitterCtlr::Update
ParticleDeferState g_state;
UInt32 g_heldLines = 4;
UInt32 g_ranLines = 2;
UInt32 g_wentLines = 12;

constexpr UInt32 kRefCountOffset = 0x04;  // NiRefObject

UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }

void TakeReference(UInt32 object) {
	if (mem::LooksLikeObjectAddress(object)) {
		_InterlockedIncrement(reinterpret_cast<volatile long*>(object + kRefCountOffset));
	}
}

// NiPointer's release: the last reference destroys it through its scalar
// deleting destructor, vtable slot 0 (as game/BowVisual.cpp lets go of its
// clone).
void LetGo(UInt32 object) {
	if (!mem::LooksLikeObjectAddress(object)) {
		return;
	}
	if (_InterlockedDecrement(reinterpret_cast<volatile long*>(object + kRefCountOffset)) == 0) {
		using DestroyFn = void(__fastcall*)(void* self, void* edx, bool freeThis);
		// The function in slot 0, not the table itself (the first build
		// called the table's address: torch-stow-cycle.txt, 2026-10-26 14:10,
		// EXCEPTION_ILLEGAL_INSTRUCTION at 0x00A85B0C).
		reinterpret_cast<DestroyFn>(Read(Read(object)))(reinterpret_cast<void*>(object), nullptr, true);
	}
}

void LetGo(const DeferredParticleUpdate& u) {
	LetGo(u.controller);
	LetGo(u.target);
}

UInt32 NowMs() {
	static long long s_frequency = 0;
	if (s_frequency == 0) {
		s_frequency = ReadPerformanceFrequency();
	}
	return s_frequency > 0 ? static_cast<UInt32>(ReadPerformanceCounter() * 1000 / s_frequency) : 0;
}

// Whether the controller's particle system hangs under the first-person
// tree (the arms' root, "Player1stPerson").
bool UnderFirstPerson(const void* controller) {
	const NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr) {
		return false;
	}
	const auto* node = *reinterpret_cast<const NiAVObject* const*>(static_cast<const UInt8*>(controller) +
	                                                                 kParticleUpdateTargetOffset);
	for (UInt32 depth = 0; depth < 32 && mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node)); ++depth) {
		if (node == root) {
			return true;
		}
		node = node->parent;
	}
	return false;
}

bool HeldBack(void* controller, float time, DeferredParticleUpdate& dropped) {
	const UInt32 now = NowMs();
	const UInt32 c = reinterpret_cast<UInt32>(controller);
	const UInt32 target = Read(c + kParticleUpdateTargetOffset);
	const ParticleCall call = DecideParticleUpdate(g_state, c, time, UnderFirstPerson(controller), now, target, &dropped);
	if (call == ParticleCall::Run) {
		return false;
	}
	if (call == ParticleCall::DeferNew) {
		// Kept alive until the pins run it (ParticlesAfterPins.h).
		TakeReference(c);
		TakeReference(target);
	}
	if (g_heldLines > 0) {
		--g_heldLines;
		OBVR_LOG("Particles: the update of a first-person particle system (controller %08X, vtable %08X, time %.3f) "
		         "held back until the hands are placed",
		         reinterpret_cast<UInt32>(controller), *static_cast<const UInt32*>(controller),
		         static_cast<double>(time));
	}
	return true;
}

void __fastcall HookedUpdate(void* controller, void* unusedEdx, float time) {
	DeferredParticleUpdate dropped;
	if (!HeldBack(controller, time, dropped)) {
		g_original(controller, unusedEdx, time);
	}
	LetGo(dropped);
}

void __fastcall HookedEmitter(void* controller, void* unusedEdx, float time) {
	DeferredParticleUpdate dropped;
	if (!HeldBack(controller, time, dropped)) {
		g_originalEmitter(controller, unusedEdx, time);
	}
	LetGo(dropped);
}

// Points one vtable slot at `hook` when it holds `expected` now.
bool PatchSlot(UInt32 slot, UInt32 expected, UInt32 hook, const char* what) {
	const UInt32 held = *reinterpret_cast<const UInt32*>(slot);
	if (held != expected) {
		OBVR_LOG("Particles: %s's vtable slot (%08X) holds %08X, not %08X as read - left alone, a held torch's fire "
		         "may be born away from the hand",
		         what, slot, held, expected);
		mem::ReportForeignCode("Particles", slot);
		return false;
	}
	if (!mem::SafeWrite(slot, &hook, sizeof(hook))) {
		OBVR_LOG("Particles: the write to %s's vtable slot %08X failed - left alone", what, slot);
		return false;
	}
	return true;
}

}  // namespace

void InstallParticlesAfterPins(bool enabled) {
	if (!enabled) {
		OBVR_LOG("Particles: off (Render.FirstPersonParticlesAfterPins=0) - first-person particles are born where "
		         "the game's own animation holds their emitter");
		return;
	}
	if (g_original != nullptr) {
		return;
	}
	// The update first: with the births held back alone the particles would
	// be born after the pins but moved a frame late.
	if (!PatchSlot(kParticleUpdateSlot, kParticleUpdateFunction, reinterpret_cast<UInt32>(&HookedUpdate),
	               "NiPSysUpdateCtlr")) {
		return;
	}
	g_original = reinterpret_cast<UpdateFn>(kParticleUpdateFunction);
	if (!PatchSlot(kParticleEmitterSlot, kParticleEmitterFunction, reinterpret_cast<UInt32>(&HookedEmitter),
	               "NiPSysEmitterCtlr")) {
		const UInt32 back = kParticleUpdateFunction;
		mem::SafeWrite(kParticleUpdateSlot, &back, sizeof(back));
		g_original = nullptr;
		return;
	}
	g_originalEmitter = reinterpret_cast<UpdateFn>(kParticleEmitterFunction);
	OBVR_LOG("Particles: NiPSysUpdateCtlr::Update (%08X) and NiPSysEmitterCtlr::Update (%08X) run a first-person "
	         "particle system's updates after the hands are placed while the pins run - a held torch's fire is born "
	         "at the torch",
	         kParticleUpdateFunction, kParticleEmitterFunction);
}

void RunDeferredParticleUpdates(bool pinsRan) {
	if (g_original == nullptr) {
		return;
	}
	DeferredParticleUpdate taken[kParticleDeferMost];
	const UInt32 count = TakeDeferredParticleUpdates(g_state, taken, kParticleDeferMost, pinsRan, NowMs());
	for (UInt32 i = 0; i < count; ++i) {
		auto* const controller = reinterpret_cast<UInt8*>(taken[i].controller);
		if (!mem::LooksLikeObjectAddress(taken[i].controller)) {
			continue;
		}
		// Held alive since it was held back: run only while the model still
		// has it - one that went meanwhile (a torch dropped, stowed,
		// unequipped) is only let go (ParticlesAfterPins.h).
		const UInt32 controllerRefs = Read(taken[i].controller + kRefCountOffset);
		const UInt32 targetRefs =
			mem::LooksLikeObjectAddress(taken[i].target) ? Read(taken[i].target + kRefCountOffset) : 0;
		if (HeldParticleUpdateRuns(controllerRefs, targetRefs)) {
			const UInt32 vtable = *reinterpret_cast<const UInt32*>(controller);
			if (vtable == kParticleUpdateCtlrVtable) {
				g_original(controller, nullptr, taken[i].time);
			} else if (vtable == kParticleEmitterCtlrVtable) {
				g_originalEmitter(controller, nullptr, taken[i].time);
			}
		} else if (g_wentLines > 0) {
			--g_wentLines;
			OBVR_LOG("Particles: a held-back update's model went before the pins (controller %08X held %u time(s), its "
			         "particle system %08X %u) - not run, OBVR's references let go",
			         taken[i].controller, controllerRefs, taken[i].target, targetRefs);
		}
		LetGo(taken[i]);
	}
	if (count > 0 && g_ranLines > 0) {
		--g_ranLines;
		OBVR_LOG("Particles: %u held-back first-person particle update(s) run after the hands were placed (pins %d)",
		         count, pinsRan ? 1 : 0);
	}
}

}  // namespace obvr::game
