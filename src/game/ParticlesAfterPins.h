#pragma once

// The first-person particles updated where the hands are (Nadi, Windows,
// 2026-10-24: "sehe aber mittig im bild noch einen feuerball der whl zur
// fackel gehört ... dreht sich mit dem Kopf mit").
//
// A held torch's fire and sparks are world-space particle systems
// (NiParticleSystem+0xC0, set by its LoadBinary at 0x00749BAC; the torch's
// "FireParticles" and "Sparks" read 1 in the harness): each particle is born
// at the emitter's world position and stays there in the world. The engine
// updates them in its own update of the first-person tree, through
// NiPSysUpdateCtlr::Update(float time) (vtable 0x00A8669C, slot 21 at
// 0x00A866F0, 0x00753BE0, which hands the particle system its time through
// its +0x98) and the births through NiPSysEmitterCtlr::Update, and that is before OBVR moves the arms to the controllers
// (game::PlaceFirstPersonArms, the hand pins). So the particles were born
// where the game's own torch idle holds the torch, up and left of the view,
// while the torch itself was drawn in the hand: a second fire that rides the
// first-person tree, the hand's torch only with its flipbook (measured
// 2026-10-25, torch-flame-ghost.txt: the emitter at 230.8 -1633.8 -129.2
// before the pins, the particles' sphere at 236.4 -1631.7 -129.5 round it,
// and the picture's fireball there). Before KeepBillboardTime (72fb0b8) the
// particles' time ran backwards and none was born, which is why it showed
// only since.
//
// The fix moves the update, not the particles: while the pins run, an update
// of a particle system under the first-person tree is held back and run with
// the same time once the hands are placed, so the particles are born at the
// torch in the hand, and trail as the engine meant them to. A frame without
// the pins disarms it, and the engine's updates run as before.
//
// Pure; the hook and the queue's run are in ParticlesAfterPins.cpp, covered
// by particles_after_pins_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kParticleUpdateCtlrVtable = 0x00A8669C;
inline constexpr UInt32 kParticleUpdateSlot = 0x00A866F0;   // slot 21, Update(float time)
inline constexpr UInt32 kParticleUpdateFunction = 0x00753BE0;
// The births are the emitter controller's, a time controller of its own on
// the same particle system (NiPSysEmitterCtlr, vtable 0x00A862C4, its Update
// in slot 21 at 0x00A86318, 0x00750260): it counts the particles due and has
// the emitter place them at the emitter object's world transform of the
// moment. Holding back the update alone left them born at the game's pose
// (the first fix's run, 2026-10-25 09:58: the second fireball unchanged).
inline constexpr UInt32 kParticleEmitterCtlrVtable = 0x00A862C4;
inline constexpr UInt32 kParticleEmitterSlot = 0x00A86318;
inline constexpr UInt32 kParticleEmitterFunction = 0x00750260;
inline constexpr UInt32 kParticleUpdateTargetOffset = 0x30;  // NiTimeController's target

inline constexpr UInt32 kParticleDeferMost = 32;
// The pins counted as running for this long after the last frame they ran
// in: a frame is 11 to 14 ms in the headset, a load or a menu far longer.
inline constexpr UInt32 kParticleDeferArmedMs = 250;

struct DeferredParticleUpdate {
	UInt32 controller = 0;
	float time = 0.0f;
};

struct ParticleDeferState {
	DeferredParticleUpdate queued[kParticleDeferMost];
	UInt32 count = 0;
	bool armed = false;      // the pins ran in the last frame
	UInt32 armedAtMs = 0;    // when
};

enum class ParticleCall : UInt8 { Run, Defer };

inline bool ParticleDeferArmed(const ParticleDeferState& s, UInt32 nowMs) {
	return s.armed && nowMs - s.armedAtMs <= kParticleDeferArmedMs;
}

inline void DropDeferredParticleUpdate(ParticleDeferState& s, UInt32 controller) {
	for (UInt32 i = 0; i < s.count; ++i) {
		if (s.queued[i].controller == controller) {
			for (UInt32 j = i + 1; j < s.count; ++j) {
				s.queued[j - 1] = s.queued[j];
			}
			--s.count;
			return;
		}
	}
}

// What the hook does with one update: a particle system outside the
// first-person tree, or a frame without the pins, runs now; one under it
// waits for the pins, with the latest time asked for.
inline ParticleCall DecideParticleUpdate(ParticleDeferState& s, UInt32 controller, float time, bool firstPerson,
                                         UInt32 nowMs) {
	if (!firstPerson || controller == 0) {
		return ParticleCall::Run;
	}
	if (!ParticleDeferArmed(s, nowMs)) {
		DropDeferredParticleUpdate(s, controller);
		return ParticleCall::Run;
	}
	for (UInt32 i = 0; i < s.count; ++i) {
		if (s.queued[i].controller == controller) {
			s.queued[i].time = time;
			return ParticleCall::Defer;
		}
	}
	if (s.count >= kParticleDeferMost) {
		return ParticleCall::Run;
	}
	s.queued[s.count].controller = controller;
	s.queued[s.count].time = time;
	++s.count;
	return ParticleCall::Defer;
}

// Once a frame, after the hands are placed (or the frame decided not to place
// them): the held-back updates, in the order they came, and whether the next
// frame holds them back again.
inline UInt32 TakeDeferredParticleUpdates(ParticleDeferState& s, DeferredParticleUpdate* out, UInt32 capacity,
                                          bool pinsRan, UInt32 nowMs) {
	UInt32 taken = 0;
	for (UInt32 i = 0; i < s.count && taken < capacity; ++i) {
		out[taken++] = s.queued[i];
	}
	s.count = 0;
	s.armed = pinsRan;
	s.armedAtMs = nowMs;
	return taken;
}

// [Render] FirstPersonParticlesAfterPins: the hook on the particle update.
void InstallParticlesAfterPins(bool enabled);
// After the hand pins of a frame: runs the held-back updates.
void RunDeferredParticleUpdates(bool pinsRan);

}  // namespace obvr::game
