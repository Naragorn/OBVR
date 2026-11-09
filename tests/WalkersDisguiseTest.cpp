// Oblivion Walkers' disguise (vr/WalkersDisguise.h): 'WGDS' parsed, a hand
// in a body's trunk, the gesture from a dead body to the chest (same hand,
// the window, the cooldown, a hand lost, switched off), and the look's ease
// in and out.

#include <cstdio>
#include <limits>

#include "vr/WalkersDisguise.h"

using namespace obvr::vr;
using obvr::NiPoint3;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestParse() {
	std::printf("'WGDS'\n");
	bool d = false;
	UInt32 one = 1;
	UInt32 zero = 0;
	UInt32 two = 2;
	UInt64 wide = 1;
	Check(ParseWalkersDisguise(&one, 4, d) && d, "1: disguised");
	Check(ParseWalkersDisguise(&zero, 4, d) && !d, "0: over");
	Check(!ParseWalkersDisguise(&two, 4, d), "2: refused");
	Check(!ParseWalkersDisguise(&wide, 8, d), "8 bytes: refused");
	Check(!ParseWalkersDisguise(nullptr, 4, d), "no data: refused");
	Check(kWalkersBloodMessage == 0x4C424757u && kWalkersDisguiseMessage == 0x53444757u, "the types as agreed");
}

void TestHandInBody() {
	std::printf("a hand in a body\n");
	const NiPoint3 spine{0.0f, 0.0f, 0.0f};
	const NiPoint3 head{0.0f, 0.0f, 30.0f};
	Check(HandInBody(NiPoint3{5.0f, 0.0f, 10.0f}, spine, head, 18.0f), "at the chest: in");
	Check(HandInBody(NiPoint3{0.0f, 10.0f, -25.0f}, spine, head, 18.0f), "at the belly, below Spine2: in");
	Check(!HandInBody(NiPoint3{0.0f, 0.0f, -60.0f}, spine, head, 18.0f), "past the pelvis: out");
	Check(!HandInBody(NiPoint3{30.0f, 0.0f, 10.0f}, spine, head, 18.0f), "beside it: out");
	Check(HandInBody(NiPoint3{0.0f, 0.0f, 40.0f}, spine, head, 18.0f), "just over the head: in (the end's round)");
	Check(!HandInBody(NiPoint3{0.0f, 0.0f, 10.0f}, spine, head, 0.0f), "no reach: never");
	Check(!HandInBody(NiPoint3{0.0f, 0.0f, 10.0f}, spine, head, std::numeric_limits<float>::quiet_NaN()),
	      "a reach that is no number: never");
	Check(HandInBody(NiPoint3{1.0f, 0.0f, 0.0f}, spine, spine, 18.0f), "the two points the same: round the point");
	Check(SegmentDistanceSquared(NiPoint3{0.0f, 3.0f, 0.0f}, spine, spine) == 9.0f, "a point segment: the distance");
}

DisguiseHandInput Hand(bool valid, UInt32 body, bool chest) {
	DisguiseHandInput h;
	h.valid = valid;
	h.deadBody = body;
	h.atChest = chest;
	return h;
}

void TestGesture() {
	std::printf("the gesture\n");
	DisguiseSettings set;
	DisguiseGestureState s;
	DisguiseHandInput hands[2] = {Hand(true, 0, false), Hand(true, 0, false)};
	// the chest alone does nothing
	hands[1] = Hand(true, 0, true);
	Check(StepDisguiseGesture(s, hands, true, 0.1f, set) == 0, "the chest without a body: nothing");
	// right hand in a body, out, at the chest
	hands[1] = Hand(true, 0xFF000801u, false);
	Check(StepDisguiseGesture(s, hands, true, 0.1f, set) == 0, "in the body: nothing yet");
	Check(s.body[1] == 0xFF000801u, "the body kept for that hand");
	hands[1] = Hand(true, 0, false);
	Check(StepDisguiseGesture(s, hands, true, 1.0f, set) == 0, "out of it: waiting");
	hands[1] = Hand(true, 0, true);
	Check(StepDisguiseGesture(s, hands, true, 1.0f, set) == 0xFF000801u, "at the chest within the window: sent");
	Check(s.body[0] == 0 && s.body[1] == 0 && s.cooldown == set.cooldownSeconds, "cleared, the cooldown on");
	// again at once: the cooldown holds it
	hands[1] = Hand(true, 0x22u, false);
	StepDisguiseGesture(s, hands, true, 0.1f, set);
	hands[1] = Hand(true, 0, true);
	Check(StepDisguiseGesture(s, hands, true, 0.1f, set) == 0, "within the cooldown: not sent");
	Check(s.body[1] == 0x22u, "the body still kept");
	Check(StepDisguiseGesture(s, hands, true, 3.0f, set) == 0x22u, "the cooldown over, still in the window: sent");

	// the other hand at the chest does not count
	s = DisguiseGestureState{};
	hands[0] = Hand(true, 0, false);
	hands[1] = Hand(true, 0x33u, false);
	StepDisguiseGesture(s, hands, true, 0.1f, set);
	hands[1] = Hand(true, 0, false);
	hands[0] = Hand(true, 0, true);
	Check(StepDisguiseGesture(s, hands, true, 0.1f, set) == 0, "the other hand at the chest: nothing");
	// the left hand works as well
	hands[0] = Hand(true, 0x44u, false);
	StepDisguiseGesture(s, hands, true, 0.1f, set);
	hands[0] = Hand(true, 0, true);
	Check(StepDisguiseGesture(s, hands, true, 0.1f, set) == 0x44u, "the left hand: sent");

	// too slow
	s = DisguiseGestureState{};
	hands[0] = Hand(true, 0, false);
	hands[1] = Hand(true, 0x55u, false);
	StepDisguiseGesture(s, hands, true, 0.1f, set);
	hands[1] = Hand(true, 0, false);
	StepDisguiseGesture(s, hands, true, 4.5f, set);
	Check(s.body[1] == 0, "past the window: forgotten");
	hands[1] = Hand(true, 0, true);
	Check(StepDisguiseGesture(s, hands, true, 0.1f, set) == 0, "the chest after it: nothing");

	// a hand lost
	hands[1] = Hand(true, 0x66u, false);
	StepDisguiseGesture(s, hands, true, 0.1f, set);
	hands[1] = Hand(false, 0, false);
	StepDisguiseGesture(s, hands, true, 0.1f, set);
	Check(s.body[1] == 0, "a hand not tracked: forgotten");

	// not allowed or switched off
	hands[1] = Hand(true, 0x77u, false);
	StepDisguiseGesture(s, hands, true, 0.1f, set);
	Check(StepDisguiseGesture(s, hands, false, 0.1f, set) == 0 && s.body[1] == 0, "not allowed: all forgotten");
	DisguiseSettings off;
	off.enabled = false;
	StepDisguiseGesture(s, hands, true, 0.1f, set);
	Check(StepDisguiseGesture(s, hands, true, 0.1f, off) == 0 && s.body[1] == 0, "switched off: all forgotten");
}

void TestLook() {
	std::printf("the look\n");
	DisguiseSettings set;
	DisguiseLook look;
	DisguiseLookOut out = StepDisguiseLook(look, false, 0.1f, set);
	Check(out.vignetteAlpha == 0.0f && out.darkenAlpha == 0.0f && !out.bloodyHands, "not disguised: nothing");
	out = StepDisguiseLook(look, true, 0.4f, set);
	Check(look.level > 0.49f && look.level < 0.51f, "half way in after half the fade");
	Check(out.vignetteAlpha > 0.0f && out.vignetteAlpha < set.vignetteStrength, "the ring part way");
	Check(out.bloodyHands, "the hands bloody while disguised");
	out = StepDisguiseLook(look, true, 1.0f, set);
	Check(look.level == 1.0f && out.vignetteAlpha == set.vignetteStrength && out.darkenAlpha == set.darken,
	      "full: the settings' strengths");
	out = StepDisguiseLook(look, false, 0.75f, set);
	Check(look.level > 0.49f && look.level < 0.51f && !out.bloodyHands, "half way out, the blood no longer laid");
	out = StepDisguiseLook(look, false, 5.0f, set);
	Check(look.level == 0.0f && out.vignetteAlpha == 0.0f, "out: gone");
	DisguiseSettings off;
	off.enabled = false;
	look.level = 1.0f;
	out = StepDisguiseLook(look, true, 5.0f, off);
	Check(look.level == 0.0f && !out.bloodyHands, "switched off: fades out though disguised");
	DisguiseSettings snap;
	snap.fadeInSeconds = 0.0f;
	snap.fadeOutSeconds = 0.0f;
	snap.bloodyHands = false;
	out = StepDisguiseLook(look, true, 0.01f, snap);
	Check(look.level == 1.0f && !out.bloodyHands, "no fade in: at once; blood off: none");
	StepDisguiseLook(look, false, 0.01f, snap);
	Check(look.level == 0.0f, "no fade out: at once");
}

bool Near(const NiPoint3& a, const NiPoint3& b) {
	return (a - b).LengthSquared() < 1e-6f;
}

void TestStainSides() {
	std::printf("the hand's stains from every side\n");
	const obvr::NiMatrix33 id = obvr::NiMatrix33::Identity();
	Check(Near(HandStainDirection(0, id, NiPoint3{0.0f, 30.0f, -40.0f}), NiPoint3{0.0f, 0.6f, -0.8f}),
	      "side 0: from the eye towards the hand, unit");
	Check(Near(HandStainDirection(0, id, NiPoint3{0.0f, 0.0f, 0.0f}), NiPoint3{0.0f, 0.0f, -1.0f}),
	      "a hand at the eye: straight down");
	Check(Near(HandStainDirection(1, id, NiPoint3{}), NiPoint3{-1.0f, 0.0f, 0.0f}) &&
	          Near(HandStainDirection(2, id, NiPoint3{}), NiPoint3{1.0f, 0.0f, 0.0f}),
	      "sides 1, 2: along the hand's x both ways");
	Check(Near(HandStainDirection(3, id, NiPoint3{}), NiPoint3{0.0f, -1.0f, 0.0f}) &&
	          Near(HandStainDirection(4, id, NiPoint3{}), NiPoint3{0.0f, 1.0f, 0.0f}),
	      "sides 3, 4: along its y");
	Check(Near(HandStainDirection(5, id, NiPoint3{}), NiPoint3{0.0f, 0.0f, -1.0f}) &&
	          Near(HandStainDirection(6, id, NiPoint3{}), NiPoint3{0.0f, 0.0f, 1.0f}),
	      "sides 5, 6: along its z");
	obvr::NiMatrix33 turned{};  // a quarter turn about z: x -> y
	turned.data[0][1] = -1.0f;
	turned.data[1][0] = 1.0f;
	turned.data[2][2] = 1.0f;
	Check(Near(HandStainDirection(2, turned, NiPoint3{}), NiPoint3{0.0f, 1.0f, 0.0f}),
	      "the hand turned: its own axis in the world");
	Check(Near(HandStainDirection(kHandStainSides, id, NiPoint3{0.0f, 0.0f, -2.0f}), NiPoint3{0.0f, 0.0f, -1.0f}),
	      "past the last side: from the eye");
}

}  // namespace

int main() {
	TestStainSides();
	TestParse();
	TestHandInBody();
	TestGesture();
	TestLook();
	if (g_failures != 0) {
		std::printf("%d failed\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
