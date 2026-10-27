#include <cstdio>
#include <cstring>

#include "render/OverlayWarmup.h"

namespace {
int failures = 0;
void Check(bool condition, const char* text) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", text);
	if (!condition) ++failures;
}

using namespace obvr::render;

constexpr std::uintptr_t kDevice = 0x1000;
constexpr std::uintptr_t kOtherDevice = 0x2000;

// The first world frame after a load, nine layers, all in one frame.
OverlayWarmupInput World(UInt32 layers = 9, UInt32 perFrame = 0) {
	OverlayWarmupInput in;
	in.enabled = true;
	in.vrRunning = true;
	in.loading = false;
	in.inWorld = true;
	in.menuIsUp = false;
	in.device = kDevice;
	in.layerCount = layers;
	in.perFrame = perFrame;
	return in;
}

OverlayWarmupInput Loading() {
	OverlayWarmupInput in = World();
	in.loading = true;
	in.inWorld = false;
	return in;
}

bool Empty(const OverlayWarmupSlice& s) { return s.count == 0; }

// A state that has seen a load on kDevice and waits for the world.
OverlayWarmupState AfterLoad() {
	OverlayWarmupState s;
	StepOverlayWarmup(s, Loading());
	return s;
}
}  // namespace

int main() {
	// --- arming
	{
		OverlayWarmupState s;
		Check(!s.pending && s.device == 0 && s.next == 0, "a new state waits for nothing");
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, Loading());
		Check(Empty(slice), "a loading frame warms nothing");
		Check(s.pending, "a loading frame arms the warm-up");
		Check(s.device == kDevice, "a loading frame notes the device it saw");
	}
	{
		OverlayWarmupState s;
		s.next = 5;
		s.device = kDevice;
		StepOverlayWarmup(s, Loading());
		Check(s.pending && s.next == 0, "a load starts the list again from its first layer");
	}
	{
		// The first device ever seen, in the world without a load first (the
		// DLL loaded into a running game): that device's textures want it too.
		OverlayWarmupState s;
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, World());
		Check(s.device == kDevice, "the first device is noted");
		Check(slice.first == 0 && slice.count == 9, "a first device warms the list at once in the world");
		Check(!s.pending, "and the warm-up is done");
	}
	{
		// A new device mid-session: the next world frame warms again.
		OverlayWarmupState s = AfterLoad();
		StepOverlayWarmup(s, World());
		Check(!s.pending, "warmed on the first device");
		OverlayWarmupInput in = World();
		in.device = kOtherDevice;
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, in);
		Check(s.device == kOtherDevice, "the new device is noted");
		Check(slice.first == 0 && slice.count == 9, "a new device warms the whole list again");
	}
	{
		// No device for a frame (not read yet) is not a new device.
		OverlayWarmupState s = AfterLoad();
		StepOverlayWarmup(s, World());
		OverlayWarmupInput in = World();
		in.device = 0;
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, in);
		Check(Empty(slice) && !s.pending && s.device == kDevice, "no device read is not a device change");
	}

	// --- idle
	{
		OverlayWarmupState s;
		s.device = kDevice;
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, World());
		Check(Empty(slice) && !s.pending, "no load seen, the same device: nothing to warm");
	}
	{
		OverlayWarmupState s = AfterLoad();
		StepOverlayWarmup(s, World());
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, World());
		Check(Empty(slice), "once warmed, the next world frame warms nothing (once per load)");
	}

	// --- the switch
	{
		OverlayWarmupState s = AfterLoad();
		OverlayWarmupInput in = World();
		in.enabled = false;
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, in);
		Check(Empty(slice), "WarmOverlays=0: nothing warmed");
		Check(!s.pending, "WarmOverlays=0: the load is let go, not held for later");
		in.enabled = true;
		Check(Empty(StepOverlayWarmup(s, in)), "switched on after that: nothing until the next load");
		StepOverlayWarmup(s, Loading());
		Check(StepOverlayWarmup(s, World()).count == 9, "the next load warms with the switch on");
	}

	// --- waiting for the world
	{
		OverlayWarmupState s = AfterLoad();
		OverlayWarmupInput in = World();
		in.inWorld = false;
		Check(Empty(StepOverlayWarmup(s, in)) && s.pending, "not in the world yet: waits");
	}
	{
		OverlayWarmupState s = AfterLoad();
		OverlayWarmupInput in = World();
		in.menuIsUp = true;
		Check(Empty(StepOverlayWarmup(s, in)) && s.pending, "a menu up: waits");
	}
	{
		OverlayWarmupState s = AfterLoad();
		OverlayWarmupInput in = World();
		in.vrRunning = false;
		Check(Empty(StepOverlayWarmup(s, in)) && s.pending, "no OpenVR: waits");
	}
	{
		OverlayWarmupState s = AfterLoad();
		OverlayWarmupInput in = World();
		in.device = 0;
		Check(Empty(StepOverlayWarmup(s, in)) && s.pending, "no game device: waits");
		Check(StepOverlayWarmup(s, World()).count == 9, "the device there: warms");
	}

	// --- an empty list
	{
		OverlayWarmupState s = AfterLoad();
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, World(0));
		Check(Empty(slice) && !s.pending, "nothing to warm: done at once");
	}

	// --- one frame (perFrame 0) and spread (perFrame n)
	{
		OverlayWarmupState s = AfterLoad();
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, World(9, 0));
		Check(slice.first == 0 && slice.count == 9 && !s.pending, "perFrame 0: all nine in the first world frame");
	}
	{
		OverlayWarmupState s = AfterLoad();
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, World(9, 20));
		Check(slice.first == 0 && slice.count == 9 && !s.pending, "perFrame above the count: all nine, done");
	}
	{
		OverlayWarmupState s = AfterLoad();
		OverlayWarmupSlice a = StepOverlayWarmup(s, World(9, 4));
		Check(a.first == 0 && a.count == 4 && s.pending, "perFrame 4: layers 0..3 first, more to come");
		OverlayWarmupSlice b = StepOverlayWarmup(s, World(9, 4));
		Check(b.first == 4 && b.count == 4 && s.pending, "then 4..7");
		OverlayWarmupSlice c = StepOverlayWarmup(s, World(9, 4));
		Check(c.first == 8 && c.count == 1 && !s.pending, "then the last one, and done");
		Check(Empty(StepOverlayWarmup(s, World(9, 4))), "nothing after that");
	}
	{
		// Spread, and a menu opens between two slices: the rest waits for it.
		OverlayWarmupState s = AfterLoad();
		StepOverlayWarmup(s, World(9, 3));
		OverlayWarmupInput menu = World(9, 3);
		menu.menuIsUp = true;
		Check(Empty(StepOverlayWarmup(s, menu)) && s.pending && s.next == 3, "a menu between slices: the rest waits");
		const OverlayWarmupSlice next = StepOverlayWarmup(s, World(9, 3));
		Check(next.first == 3 && next.count == 3, "and goes on where it stopped");
	}
	{
		// Spread, and a load comes between two slices: from the start again.
		OverlayWarmupState s = AfterLoad();
		StepOverlayWarmup(s, World(9, 3));
		StepOverlayWarmup(s, Loading());
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, World(9, 3));
		Check(slice.first == 0 && slice.count == 3, "a load between slices starts the list again");
	}
	{
		// The list got shorter between two slices (a setting went off): done
		// rather than reaching past its end.
		OverlayWarmupState s = AfterLoad();
		StepOverlayWarmup(s, World(9, 5));
		const OverlayWarmupSlice slice = StepOverlayWarmup(s, World(4, 5));
		Check(Empty(slice) && !s.pending, "a list shorter than where it stood: done, nothing past its end");
	}

	// --- the result names
	Check(std::strcmp(OverlayWarmResultName(OverlayWarmResult::Handed), "handed to the compositor") == 0,
	      "Handed reads 'handed to the compositor'");
	Check(std::strcmp(OverlayWarmResultName(OverlayWarmResult::AlreadyWarm), "already warm") == 0,
	      "AlreadyWarm reads 'already warm'");
	Check(std::strcmp(OverlayWarmResultName(OverlayWarmResult::Failed), "NOT handed (its reason above)") == 0,
	      "Failed reads 'NOT handed'");
	Check(std::strcmp(OverlayWarmResultName(static_cast<OverlayWarmResult>(99)), "?") == 0,
	      "an unknown result reads '?'");

	// --- which layers are on the list
	{
		OverlayWarmWants none;
		Check(!OverlayWarmWanted(OverlayWarmKind::FullVrPanel, none), "Full VR off: no quick menu, ring or panels");
		Check(!OverlayWarmWanted(OverlayWarmKind::EnemyHealth, none), "health bars off: none of them");
		Check(!OverlayWarmWanted(OverlayWarmKind::Vignette, none), "both vignettes off: no vignette");
		Check(!OverlayWarmWanted(OverlayWarmKind::Crosshair, none), "the crosshair off: no crosshair");
		Check(!OverlayWarmWanted(static_cast<OverlayWarmKind>(99), none), "an unknown kind: not wanted");
		OverlayWarmWants fullVr;
		fullVr.fullVr = true;
		Check(OverlayWarmWanted(OverlayWarmKind::FullVrPanel, fullVr), "Full VR on: its panels");
		Check(!OverlayWarmWanted(OverlayWarmKind::EnemyHealth, fullVr), "Full VR alone does not bring the bars");
		OverlayWarmWants bars;
		bars.enemyHealth = true;
		Check(OverlayWarmWanted(OverlayWarmKind::EnemyHealth, bars), "health bars on: the bars");
		Check(!OverlayWarmWanted(OverlayWarmKind::FullVrPanel, bars), "the bars alone do not bring Full VR's panels");
		OverlayWarmWants vignette;
		vignette.vignette = true;
		Check(OverlayWarmWanted(OverlayWarmKind::Vignette, vignette), "a vignette on: the vignette");
		Check(!OverlayWarmWanted(OverlayWarmKind::Crosshair, vignette), "the vignette alone does not bring the crosshair");
		OverlayWarmWants crosshair;
		crosshair.crosshair = true;
		Check(OverlayWarmWanted(OverlayWarmKind::Crosshair, crosshair), "the crosshair on: the crosshair");
		Check(!OverlayWarmWanted(OverlayWarmKind::Vignette, crosshair), "the crosshair alone does not bring the vignette");

		Check(!OverlayWarmWanted(OverlayWarmKind::SettingsMenu, none), "the settings menu not wanted: not warmed");
		OverlayWarmWants settings;
		settings.settingsMenu = true;
		Check(OverlayWarmWanted(OverlayWarmKind::SettingsMenu, settings),
		      "the settings menu wanted: its 1024x768 texture warmed (100 ms at its first open, 2026-10-27)");
		Check(!OverlayWarmWanted(OverlayWarmKind::FullVrPanel, settings), "the settings menu alone brings no panels");
		Check(!OverlayWarmWanted(OverlayWarmKind::SettingsMenu, fullVr), "Full VR alone does not bring the settings menu");
	}

	// --- whether a hand-over in play was not warm
	Check(HandedInPlay(false), "a first hand-over in play: not warmed");
	Check(!HandedInPlay(true), "a hand-over of a warmed texture: nothing to say");

	std::printf(failures == 0 ? "all passed\n" : "%d failed\n", failures);
	return failures == 0 ? 0 : 1;
}
