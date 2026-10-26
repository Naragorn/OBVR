#pragma once

#include "core/Types.h"

namespace obvr::render {

// The actor shadow maps drawn once for both eyes ([Render] ShareActorShadows).
//
// Oblivion draws its actor shadows in addr::kRenderActorShadows, which the
// scene render (addr::kRenderScene) calls once a render - so the dual pass
// draws every caster's shadow map twice a frame. A shadow map is the caster
// seen from its light: it does not depend on the eye, only the receivers do,
// and they sample the maps in each eye's own scene pass. So the second eye
// can keep the maps the first eye drew in the same frame. Measured with
// eighteen Walkers standing in view (OblivionWalkers perf-walkers-in-view,
// 2026-10-26, iActorShadowCountInt 0 against 10): ten casters cost 1.4 ms a
// scene pass, 2.8 ms a stereo frame. FNVVR Reborn does the same for New
// Vegas (d3d9.vrShareShadows, its vr_render_passes.h).
//
// The rule: the second eye reuses only maps the first eye drew in the same
// world render (the same scene call); in every other case - off, a single
// pass, a first eye that never asked - the engine's call runs as before.

enum class ActorShadowPlan : UInt8 {
	Draw = 0,   // call the engine's actor shadow render
	Reuse = 1,  // skip it: the first eye's maps of this frame stay
};

struct ShadowShareState {
	bool firstEyeDrew = false;      // the first eye drew the maps ...
	UInt32 firstEyeSceneCall = 0;   // ... in this world render
	UInt32 drawn = 0;
	UInt32 reused = 0;
};

// One ask of the engine: `pass` is the dual pass's eye (0 first, 1 second,
// -1 a single-pass render), `sceneCall` the world render it belongs to.
inline ActorShadowPlan PlanActorShadows(ShadowShareState& state, bool enabled, int pass, UInt32 sceneCall) {
	const bool reuse = enabled && pass == 1 && state.firstEyeDrew && state.firstEyeSceneCall == sceneCall;
	if (reuse) {
		++state.reused;
		return ActorShadowPlan::Reuse;
	}
	++state.drawn;
	if (pass == 0) {
		state.firstEyeDrew = true;
		state.firstEyeSceneCall = sceneCall;
	}
	return ActorShadowPlan::Draw;
}

}  // namespace obvr::render
