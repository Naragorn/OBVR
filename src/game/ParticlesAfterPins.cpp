#include "game/ParticlesAfterPins.h"

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

bool HeldBack(void* controller, float time) {
	const UInt32 now = NowMs();
	if (DecideParticleUpdate(g_state, reinterpret_cast<UInt32>(controller), time, UnderFirstPerson(controller), now) !=
	    ParticleCall::Defer) {
		return false;
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
	if (!HeldBack(controller, time)) {
		g_original(controller, unusedEdx, time);
	}
}

void __fastcall HookedEmitter(void* controller, void* unusedEdx, float time) {
	if (!HeldBack(controller, time)) {
		g_originalEmitter(controller, unusedEdx, time);
	}
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
		// Still a particle update controller: the frame's own, unless its
		// model went between the engine's update and the pins.
		if (!mem::LooksLikeObjectAddress(taken[i].controller)) {
			continue;
		}
		const UInt32 vtable = *reinterpret_cast<const UInt32*>(controller);
		if (vtable == kParticleUpdateCtlrVtable) {
			g_original(controller, nullptr, taken[i].time);
		} else if (vtable == kParticleEmitterCtlrVtable) {
			g_originalEmitter(controller, nullptr, taken[i].time);
		}
	}
	if (count > 0 && g_ranLines > 0) {
		--g_ranLines;
		OBVR_LOG("Particles: %u held-back first-person particle update(s) run after the hands were placed (pins %d)",
		         count, pinsRan ? 1 : 0);
	}
}

}  // namespace obvr::game
