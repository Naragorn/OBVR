// Checks what the 2D layer's overlay shows and where it hangs as the tablet
// comes and goes (render/HudPlacementLogic.h).
//
// Nadi's Windows run of 2026-11-22: after the tablet had been up the Esc menu
// was cut off on the right and its buttons could not be hit, and a box that
// came up after the tablet went down hung "somewhere in the room". The frames
// below are the run's: the tablet held, the overlay shown on the first frame
// and then hidden while the scene draws the tablet (its placement given every
// frame all the same), the tablet put down, a menu on the layer's quad.

#include <cstdio>

#include "render/HudPlacementLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::render::HudBoundsAction;
using obvr::render::HudBoundsState;
using obvr::render::KeepAnchorOffWrist;
using obvr::render::StepHudBounds;
using obvr::vr::openvr::VRTextureBounds;

const VRTextureBounds kContent{0.0f, 0.0f, 0.5f, 0.672f};   // the 4036x2270 the 2D draws in, of 8072x3376
const VRTextureBounds kTablet{0.0078f, 0.0447f, 0.2261f, 0.6464f};  // the run's crop, 63..1825 x 151..2182

// The compositor, as the overlay's calls leave it.
struct Compositor {
	VRTextureBounds bounds = kContent;
	void Apply(HudBoundsAction a, const VRTextureBounds& tablet) {
		if (a == HudBoundsAction::Tablet) {
			bounds = tablet;
		} else if (a == HudBoundsAction::Content) {
			bounds = kContent;
		}
	}
};

// The flag of f189392, as HudLayer had it: cleared by the tablet's placement
// every frame, read only when the overlay is placed. Kept as the record of
// the failure.
struct OldHud {
	bool applied = false;
	bool tabletSet = false;
	void PlaceTablet() {  // SetTabletPlacement after SetWristPlacement cleared tabletSet
		if (!tabletSet) {
			applied = false;
		}
		tabletSet = true;
	}
	HudBoundsAction Place(bool tablet) {
		if (tablet && !applied) {
			applied = true;
			return HudBoundsAction::Tablet;
		}
		if (!tablet && applied) {
			applied = false;
			return HudBoundsAction::Content;
		}
		return HudBoundsAction::None;
	}
};

bool Same(const VRTextureBounds& a, const VRTextureBounds& b) {
	return obvr::render::SameBounds(a, b);
}

void TestTheRunsFrames() {
	std::printf("The tablet held, hidden while drawn into the eyes, put down, the Esc menu\n");
	Compositor now;
	HudBoundsState s;
	now.Apply(StepHudBounds(s, true, kTablet), kTablet);  // the first frame: the overlay shown and placed
	Check(Same(now.bounds, kTablet), "the tablet's first frame: its cut");
	// Hidden while the scene draws it: not placed, nothing sent.
	now.Apply(StepHudBounds(s, false, kContent), kTablet);  // put down, the Esc menu placed on the quad
	Check(Same(now.bounds, kContent), "put down: the layer's own bounds back - the Esc menu whole");

	// The same frames with the old flag.
	Compositor old;
	OldHud hud;
	hud.tabletSet = false;
	hud.PlaceTablet();
	old.Apply(hud.Place(true), kTablet);
	for (int frame = 0; frame < 5; ++frame) {
		hud.tabletSet = false;  // SetWristPlacement, every frame
		hud.PlaceTablet();      // hidden: not placed
	}
	hud.tabletSet = false;  // ClearWristPlacement as it is put down
	old.Apply(hud.Place(false), kTablet);
	Check(Same(old.bounds, kTablet), "the old flag: the tablet's cut kept on the Esc menu (the failing run)");
}

void TestBounds() {
	std::printf("The bounds sent\n");
	HudBoundsState s;
	Check(StepHudBounds(s, false, kContent) == HudBoundsAction::None, "no tablet ever: nothing sent");
	Check(StepHudBounds(s, true, kTablet) == HudBoundsAction::Tablet, "the tablet: its cut");
	Check(StepHudBounds(s, true, kTablet) == HudBoundsAction::None, "the same cut again: nothing sent");
	VRTextureBounds wider = kTablet;
	wider.uMax = 0.4f;  // a popup joined
	Check(StepHudBounds(s, true, wider) == HudBoundsAction::Tablet, "the cut grows: sent again");
	Check(StepHudBounds(s, false, kContent) == HudBoundsAction::Content, "the tablet gone: the layer's own");
	Check(StepHudBounds(s, false, kContent) == HudBoundsAction::None, "sent once");
	Check(StepHudBounds(s, true, wider) == HudBoundsAction::Tablet, "the tablet back with the cut it had: sent again");
}

void TestAnchor() {
	std::printf("The room anchor off the wrist\n");
	Check(!KeepAnchorOffWrist(true, true, true, false),
	      "off the tablet, menus in the room: the anchor dropped, taken again where the head looks");
	Check(KeepAnchorOffWrist(true, true, true, true), "an anchor placed on purpose meanwhile (a conversation): kept");
	Check(KeepAnchorOffWrist(true, false, true, false),
	      "ahead of the head: kept marked, so the head-relative transform goes back");
	Check(KeepAnchorOffWrist(false, true, true, false), "not leaving the wrist: the room anchor stays");
	Check(!KeepAnchorOffWrist(false, true, false, false), "no anchor: none");
}

}  // namespace

int main() {
	TestTheRunsFrames();
	TestBounds();
	TestAnchor();
	if (g_failures != 0) {
		std::printf("\n%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("\nall checks passed\n");
	return 0;
}
