#include <cstdio>

#include "render/ShadowShare.h"

using obvr::render::ActorShadowPlan;
using obvr::render::PlanActorShadows;
using obvr::render::ShadowShareState;

namespace {

unsigned g_failures = 0;
unsigned g_checks = 0;

void Check(bool condition, const char* message) {
	++g_checks;
	if (!condition) {
		++g_failures;
		std::printf("FAIL: %s\n", message);
	}
}

// One world render of the dual pass: the first eye, then the second, the
// engine asking for the actor shadows once in each (or not at all, when its
// shadows are off or no actor casts one).
void DualFrame(ShadowShareState& state, bool enabled, UInt32 sceneCall, bool firstAsks, bool secondAsks,
               ActorShadowPlan* first, ActorShadowPlan* second) {
	if (firstAsks) *first = PlanActorShadows(state, enabled, 0, sceneCall);
	if (secondAsks) *second = PlanActorShadows(state, enabled, 1, sceneCall);
}

}  // namespace

int main() {
	// Off (the default): both eyes draw their own shadow maps, as before.
	{
		ShadowShareState state;
		ActorShadowPlan first = ActorShadowPlan::Reuse, second = ActorShadowPlan::Reuse;
		DualFrame(state, false, 10, true, true, &first, &second);
		Check(first == ActorShadowPlan::Draw && second == ActorShadowPlan::Draw,
		      "off: both eyes draw the shadow maps");
		Check(state.drawn == 2 && state.reused == 0, "off: two draws counted, nothing reused");
	}
	// On: the first eye draws, the second keeps the first eye's maps.
	{
		ShadowShareState state;
		ActorShadowPlan first = ActorShadowPlan::Reuse, second = ActorShadowPlan::Draw;
		DualFrame(state, true, 10, true, true, &first, &second);
		Check(first == ActorShadowPlan::Draw, "on: the first eye draws");
		Check(second == ActorShadowPlan::Reuse, "on: the second eye reuses the first eye's maps");
		Check(state.drawn == 1 && state.reused == 1, "on: one draw, one reuse");
	}
	// On, frame after frame: every frame's second eye reuses its own frame's
	// first-eye maps, never an older frame's.
	{
		ShadowShareState state;
		bool everyFrame = true;
		for (UInt32 call = 1; call <= 50; ++call) {
			ActorShadowPlan first = ActorShadowPlan::Reuse, second = ActorShadowPlan::Draw;
			DualFrame(state, true, call, true, true, &first, &second);
			everyFrame = everyFrame && first == ActorShadowPlan::Draw && second == ActorShadowPlan::Reuse;
		}
		Check(everyFrame && state.drawn == 50 && state.reused == 50, "on, 50 frames: one draw and one reuse each");
	}
	// The first eye did not draw this frame (the engine never asked: its
	// shadows went off, nothing cast one) but did last frame: the second eye
	// draws - last frame's maps are not this frame's.
	{
		ShadowShareState state;
		ActorShadowPlan first = ActorShadowPlan::Reuse, second = ActorShadowPlan::Draw;
		DualFrame(state, true, 20, true, true, &first, &second);
		second = ActorShadowPlan::Reuse;
		DualFrame(state, true, 21, false, true, &first, &second);
		Check(second == ActorShadowPlan::Draw, "the first eye skipped this frame: the second eye draws its own");
	}
	// A single-pass render (aer, flat, a menu probe): the pass is -1 and the
	// engine's call always runs.
	{
		ShadowShareState state;
		const ActorShadowPlan single = PlanActorShadows(state, true, -1, 30);
		Check(single == ActorShadowPlan::Draw, "single pass: drawn");
		const ActorShadowPlan again = PlanActorShadows(state, true, -1, 31);
		Check(again == ActorShadowPlan::Draw && state.reused == 0, "single pass, the next frame: drawn again, never reused");
	}
	// The engine asked twice in one eye's pass (not seen in 1.2.416, which
	// calls it once from the scene render): a second ask in the second eye
	// still reuses, a second ask in the first eye draws.
	{
		ShadowShareState state;
		const ActorShadowPlan a = PlanActorShadows(state, true, 0, 40);
		const ActorShadowPlan b = PlanActorShadows(state, true, 0, 40);
		const ActorShadowPlan c = PlanActorShadows(state, true, 1, 40);
		const ActorShadowPlan d = PlanActorShadows(state, true, 1, 40);
		Check(a == ActorShadowPlan::Draw && b == ActorShadowPlan::Draw, "two asks in the first eye: both drawn");
		Check(c == ActorShadowPlan::Reuse && d == ActorShadowPlan::Reuse, "two asks in the second eye: both reused");
	}
	// Switched on between the eyes (a hot reload): the first eye's draw was
	// noted though sharing was off, so the second eye may reuse it - the maps
	// are this frame's either way.
	{
		ShadowShareState state;
		const ActorShadowPlan first = PlanActorShadows(state, false, 0, 50);
		const ActorShadowPlan second = PlanActorShadows(state, true, 1, 50);
		Check(first == ActorShadowPlan::Draw && second == ActorShadowPlan::Reuse,
		      "switched on between the eyes: the first eye's maps of this frame reused");
	}
	// Switched off between the eyes: the second eye draws.
	{
		ShadowShareState state;
		const ActorShadowPlan first = PlanActorShadows(state, true, 0, 60);
		const ActorShadowPlan second = PlanActorShadows(state, false, 1, 60);
		Check(first == ActorShadowPlan::Draw && second == ActorShadowPlan::Draw, "switched off between the eyes: drawn");
	}
	// The scene call counter wraps: a first eye at 0xFFFFFFFF and a second
	// eye at 0 are different frames.
	{
		ShadowShareState state;
		PlanActorShadows(state, true, 0, 0xFFFFFFFFu);
		const ActorShadowPlan second = PlanActorShadows(state, true, 1, 0);
		Check(second == ActorShadowPlan::Draw, "a wrapped scene call is another frame: drawn");
	}
	// A fresh state (no frame drawn yet) at scene call 0: the second eye draws.
	{
		ShadowShareState state;
		const ActorShadowPlan second = PlanActorShadows(state, true, 1, 0);
		Check(second == ActorShadowPlan::Draw, "nothing drawn yet, scene call 0: drawn");
	}
	// The mirror's pass (docs/mirror-spec.md stage 2) after a single-pass world
	// render of the same scene call: it keeps that render's maps.
	{
		ShadowShareState state;
		const ActorShadowPlan world = PlanActorShadows(state, true, -1, 70);
		const ActorShadowPlan mirror = PlanActorShadows(state, true, -1, 70, true);
		Check(world == ActorShadowPlan::Draw && mirror == ActorShadowPlan::Reuse,
		      "mirror after a single-pass render: the render's maps reused");
		Check(state.drawn == 1 && state.reused == 1, "mirror: one draw, one reuse");
	}
	// After a dual frame: the mirror keeps the maps the eyes drew.
	{
		ShadowShareState state;
		PlanActorShadows(state, true, 0, 80);
		PlanActorShadows(state, true, 1, 80);
		const ActorShadowPlan mirror = PlanActorShadows(state, true, -1, 80, true);
		Check(mirror == ActorShadowPlan::Reuse, "mirror after a dual frame: reused");
	}
	// Sharing off: the mirror draws its own.
	{
		ShadowShareState state;
		PlanActorShadows(state, false, -1, 90);
		const ActorShadowPlan mirror = PlanActorShadows(state, false, -1, 90, true);
		Check(mirror == ActorShadowPlan::Draw, "mirror, sharing off: drawn");
	}
	// The world render of this frame did not draw them (the engine never asked)
	// but last frame's did: the mirror draws - last frame's maps are not this frame's.
	{
		ShadowShareState state;
		PlanActorShadows(state, true, -1, 100);
		const ActorShadowPlan mirror = PlanActorShadows(state, true, -1, 101, true);
		Check(mirror == ActorShadowPlan::Draw, "mirror at another scene call: drawn");
	}
	// A mirror that drew its own maps does not stand for the world's: a second
	// mirror ask in a scene call whose world drew nothing draws again.
	{
		ShadowShareState state;
		const ActorShadowPlan a = PlanActorShadows(state, true, -1, 110, true);
		const ActorShadowPlan b = PlanActorShadows(state, true, -1, 110, true);
		Check(a == ActorShadowPlan::Draw && b == ActorShadowPlan::Draw, "mirror only: drawn each time");
	}
	// Stage 4: two mirror passes after the frame's world render both keep its
	// maps, and the mirror's kept maps are counted for the cost line.
	{
		ShadowShareState state;
		PlanActorShadows(state, true, -1, 130);
		const ActorShadowPlan left = PlanActorShadows(state, true, -1, 130, true);
		const ActorShadowPlan right = PlanActorShadows(state, true, -1, 130, true);
		Check(left == ActorShadowPlan::Reuse && right == ActorShadowPlan::Reuse,
		      "two mirror passes: both keep the world render's maps");
		Check(state.drawn == 1 && state.mirrorKept == 2, "the maps drawn once, kept for the mirror twice");
		PlanActorShadows(state, true, 0, 131);
		PlanActorShadows(state, true, 1, 131);
		Check(state.mirrorKept == 2, "the dual pass's second eye is not counted as the mirror's");
	}
	// The world render's note changes nothing for the dual pass: a single-pass
	// render's maps are never a second eye's (another scene call), and a first
	// eye still decides alone.
	{
		ShadowShareState state;
		PlanActorShadows(state, true, -1, 120);
		const ActorShadowPlan second = PlanActorShadows(state, true, 1, 120);
		Check(second == ActorShadowPlan::Draw, "a second eye after a single-pass render's note: drawn as before");
	}

	std::printf("shadow share: %u checks, %u failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
