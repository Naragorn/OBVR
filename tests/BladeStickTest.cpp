// The stab that sticks (game/BladeStickLogic.h): how it starts, the pin
// along the axis with its easing, pulled out, torn out, lost, a load, off,
// the ragdoll's drive and its tear, a death while standing.

#include <cmath>
#include <cstdio>
#include <string_view>

#include "game/BladeStickLogic.h"

namespace {

using namespace obvr;
using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tol = 0.05f) { return std::fabs(a - b) <= tol; }
bool NearP(const NiPoint3& a, const NiPoint3& b, float tol = 0.05f) {
	return Near(a.x, b.x, tol) && Near(a.y, b.y, tol) && Near(a.z, b.z, tol);
}

// A blade 40 units long along the grip's +x, the guard at the grip.
BodySpan Span() {
	BodySpan s;
	s.valid = true;
	s.a = NiPoint3{0.0f, 0.0f, 0.0f};
	s.b = NiPoint3{40.0f, 0.0f, 0.0f};
	return s;
}

BladePose PoseAt(float x, float y = 0.0f, float z = 0.0f) {
	BladePose p;
	p.pos = NiPoint3{x, y, z};
	return p;
}

StickAnchor Anchor(float x = 50.0f) {
	StickAnchor a;
	a.valid = true;
	a.pos = NiPoint3{x, 0.0f, 0.0f};
	return a;
}

// The blade held with its tip 5 units past the entry point at x 50: the
// grip at x 15, the tip at 55.
BladeStickState Stuck(StickMode mode) {
	BladeStickState st;
	StartBladeStick(st, mode, 0x1234u, 7u, Anchor(), PoseAt(15.0f), Span(), NiPoint3{50.0f, 0.0f, 0.0f});
	return st;
}

BladeStickFrame Frame(const BladePose& wanted, float dt = 1.0f / 90.0f) {
	BladeStickFrame f;
	f.anchor = Anchor();
	f.wanted = wanted;
	f.span = Span();
	f.worldNow = 7u;
	f.dtSeconds = dt;
	return f;
}

void TestStart() {
	std::printf("how a stab that went in starts\n");
	BladeStickSettings s;
	Check(StickStartFor(s, true, true, BodyPart::LeftArm, 9.0f) == StickMode::Standing, "the living, an arm: standing");
	Check(StickStartFor(s, true, true, BodyPart::Torso, 2.0f) == StickMode::Standing, "the living, the torso, slow: standing");
	Check(StickStartFor(s, true, true, BodyPart::Torso, 6.0f) == StickMode::Ragdoll,
	      "the living, the torso, past the ragdoll's speed: down, hanging on the blade");
	Check(StickStartFor(s, true, false, BodyPart::LeftLeg, 1.0f) == StickMode::Ragdoll, "a corpse: the ragdoll");
	Check(StickStartFor(s, false, true, BodyPart::Torso, 9.0f) == StickMode::None, "not a stab that went in: nothing");
	BladeStickSettings off;
	off.enabled = false;
	Check(StickStartFor(off, true, true, BodyPart::Torso, 9.0f) == StickMode::None, "switched off: nothing");
	BladeStickSettings never;
	never.ragdollSpeedMetres = -1.0f;
	Check(StickStartFor(never, true, true, BodyPart::Torso, 50.0f) == StickMode::Standing,
	      "the ragdoll's speed below 0: the torso stays standing");

	BladeStickState st;
	Check(StartBladeStick(st, StickMode::Standing, 1u, 7u, Anchor(), PoseAt(15.0f), Span(), NiPoint3{50.0f, 0.0f, 0.0f}) &&
	          st.mode == StickMode::Standing && Near(st.depth, 5.0f) && Near(st.maxDepth, 40.0f) &&
	          NearP(st.entryLocal, NiPoint3{0.0f, 0.0f, 0.0f}) && NearP(st.axisStart, NiPoint3{1.0f, 0.0f, 0.0f}),
	      "started: the entry point and axis in the bone's frame, the tip 5 units in, the blade's length the most");
	BladeStickState none;
	StickAnchor bad = Anchor();
	bad.valid = false;
	Check(!StartBladeStick(none, StickMode::Standing, 1u, 7u, bad, PoseAt(15.0f), Span(), NiPoint3{50.0f, 0.0f, 0.0f}) &&
	          none.mode == StickMode::None,
	      "no bone to hold on to: not started");
	BodySpan noSpan;
	Check(!StartBladeStick(none, StickMode::Standing, 1u, 7u, Anchor(), PoseAt(15.0f), noSpan, NiPoint3{50.0f, 0.0f, 0.0f}),
	      "no blade read: not started");
	Check(!StartBladeStick(none, StickMode::None, 1u, 7u, Anchor(), PoseAt(15.0f), Span(), NiPoint3{50.0f, 0.0f, 0.0f}),
	      "no mode: not started");
	BladeStickState shallow;
	StartBladeStick(shallow, StickMode::Standing, 1u, 7u, Anchor(), PoseAt(5.0f), Span(), NiPoint3{50.0f, 0.0f, 0.0f});
	Check(Near(shallow.depth, 0.0f), "the tip short of the entry point: depth 0, not below");
}

void TestStanding() {
	std::printf("standing: the pin along the axis\n");
	BladeStickSettings s;
	{
		BladeStickState st = Stuck(StickMode::Standing);
		const BladeStickStep r = StepBladeStick(st, s, Frame(PoseAt(15.0f, 6.0f, 0.0f)));
		Check(r.posed && r.end == StickEnd::None && Near(r.pose.pos.y, 0.0f) && Near(r.pose.pos.x + 40.0f, 55.0f),
		      "the hand 6 units aside: the blade stays on its axis, its tip where it was");
	}
	{
		BladeStickState st = Stuck(StickMode::Standing);
		BladeStickStep r;
		for (int i = 0; i < 200; ++i) {
			r = StepBladeStick(st, s, Frame(PoseAt(30.0f)));
		}
		Check(r.posed && Near(st.depth, 20.0f, 0.2f), "pushed 15 units further: eased to 20 deep");
		BladeStickState st2 = Stuck(StickMode::Standing);
		StepBladeStick(st2, s, Frame(PoseAt(30.0f)));
		Check(st2.depth > 5.0f && st2.depth < 20.0f, "one frame of a push: on its way, not there at once (the damper in)");
	}
	{
		BladeStickState st = Stuck(StickMode::Standing);
		for (int i = 0; i < 400; ++i) {
			StepBladeStick(st, s, Frame(PoseAt(200.0f)));
		}
		Check(Near(st.depth, 40.0f, 0.2f), "pushed past the hilt: no deeper than the blade's length");
	}
	{
		BladeStickState st = Stuck(StickMode::Standing);
		const BladeStickStep r = StepBladeStick(st, s, Frame(PoseAt(10.5f)));
		Check(r.end == StickEnd::None && r.posed && st.depth < 5.0f, "drawn back but still in: comes back out, eased");
	}
	{
		BladeStickState st = Stuck(StickMode::Standing);
		const BladeStickStep r = StepBladeStick(st, s, Frame(PoseAt(8.0f)));
		Check(r.end == StickEnd::PulledOut && st.mode == StickMode::None && !r.posed,
		      "the tip 2 units back past the surface: pulled out, free");
	}
	{
		BladeStickState st = Stuck(StickMode::Standing);
		const BladeStickStep r = StepBladeStick(st, s, Frame(PoseAt(15.0f, 20.0f, 0.0f)));
		Check(r.end == StickEnd::TornOut && st.mode == StickMode::None, "the hand 20 units off the axis: torn out");
	}
	{
		BladeStickState st = Stuck(StickMode::Standing);
		BladePose turned = PoseAt(15.0f);
		const float a = 60.0f * math::kDegreesToRadians;
		turned.rot.data[0][0] = math::Cos(a);
		turned.rot.data[0][1] = -math::Sin(a);
		turned.rot.data[1][0] = math::Sin(a);
		turned.rot.data[1][1] = math::Cos(a);
		// Turned about the tip's place so the tip stays near the axis.
		turned.pos = NiPoint3{55.0f, 0.0f, 0.0f} - turned.rot * NiPoint3{40.0f, 0.0f, 0.0f};
		const BladeStickStep r = StepBladeStick(st, s, Frame(turned));
		Check(r.end == StickEnd::TornOut, "the blade turned 60 degrees off the axis: torn out");
	}
	{
		BladeStickState st = Stuck(StickMode::Standing);
		BladeStickFrame f = Frame(PoseAt(25.0f, 0.0f, 3.0f));
		f.anchor.pos = NiPoint3{60.0f, 0.0f, 0.0f};
		const BladeStickStep r = StepBladeStick(st, s, f);
		Check(r.posed && Near(r.entry.x, 60.0f) && Near(r.pose.pos.x + 40.0f, 65.0f, 0.1f) && Near(r.pose.pos.z, 0.0f),
		      "the body steps back 10 units and the hand with it: the pin goes with its bone, still on its axis");
		BladeStickState st2 = Stuck(StickMode::Standing);
		BladeStickFrame g = Frame(PoseAt(15.0f));
		g.anchor.pos = NiPoint3{60.0f, 0.0f, 0.0f};
		Check(StepBladeStick(st2, s, g).end == StickEnd::PulledOut,
		      "the body steps back 10 units off a hand that stays: the blade comes out");
	}
}

void TestBodyMovesUnderTheBlade() {
	std::printf("the body moving under the blade (its stance, a stagger)\n");
	BladeStickSettings s;
	BladeStickState st = Stuck(StickMode::Standing);
	BladeStickFrame f = Frame(PoseAt(15.0f));
	// The struck bone turned 60 degrees about the entry point (a fighting
	// stance), the hand still: the blade stays in the hand's turn, its line
	// through the entry point.
	const float a = 60.0f * math::kDegreesToRadians;
	f.anchor.rot.data[0][0] = math::Cos(a);
	f.anchor.rot.data[0][1] = -math::Sin(a);
	f.anchor.rot.data[1][0] = math::Sin(a);
	f.anchor.rot.data[1][1] = math::Cos(a);
	const BladeStickStep r = StepBladeStick(st, s, f);
	Check(r.end == StickEnd::None && r.posed && Near(r.pose.rot.data[0][0], 1.0f) && Near(r.pose.pos.y, 0.0f),
	      "the bone turned 60 degrees: not torn out, the blade keeps the hand's turn and its line");
	BladeStickFrame g = Frame(PoseAt(15.0f, 0.0f, 0.0f));
	g.anchor.pos = NiPoint3{50.0f, 6.0f, 0.0f};
	const BladeStickStep q = StepBladeStick(st, s, g);
	Check(q.end == StickEnd::None && q.posed && Near(q.lateralUnits, 6.0f, 0.1f) && Near(q.pose.pos.y, 6.0f, 0.1f),
	      "the entry point moved 6 units aside: the blade goes with it, the hand 6 off its line (within the tear)");
}

void TestEnds() {
	std::printf("the other ends\n");
	BladeStickSettings s;
	{
		BladeStickState st = Stuck(StickMode::Standing);
		BladeStickFrame f = Frame(PoseAt(15.0f));
		f.worldNow = 8u;
		Check(StepBladeStick(st, s, f).end == StickEnd::Load && st.mode == StickMode::None, "a load since: ended, unread");
	}
	{
		BladeStickState st = Stuck(StickMode::Ragdoll);
		BladeStickFrame f = Frame(PoseAt(15.0f));
		f.anchor.valid = false;
		Check(StepBladeStick(st, s, f).end == StickEnd::Lost, "the bone no longer reads: lost");
	}
	{
		BladeStickState st = Stuck(StickMode::Standing);
		BladeStickFrame f = Frame(PoseAt(15.0f));
		f.enabled = false;
		Check(StepBladeStick(st, s, f).end == StickEnd::Off, "switched off while in: ended");
	}
	{
		BladeStickState st;
		const BladeStickStep r = StepBladeStick(st, s, Frame(PoseAt(15.0f)));
		Check(r.mode == StickMode::None && r.end == StickEnd::None && !r.posed && !r.drive, "nothing stuck: nothing");
	}
	Check(std::string_view(StickEndName(StickEnd::PulledOut)) == "pulled out" &&
	          std::string_view(StickEndName(StickEnd::None)) == "-",
	      "the ends have names for the log");
}

void TestRagdoll() {
	std::printf("the ragdoll: the body hangs on the blade\n");
	BladeStickSettings s;
	{
		BladeStickState st = Stuck(StickMode::Ragdoll);
		const BladeStickStep r = StepBladeStick(st, s, Frame(PoseAt(25.0f, 8.0f, 0.0f)));
		Check(r.drive && !r.posed && NearP(r.shift, NiPoint3{0.0f, 8.0f, 0.0f}) && Near(st.depth, 15.0f),
		      "the hand moves 10 on and 8 aside: the body follows aside, the blade goes 10 deeper");
	}
	{
		BladeStickState st = Stuck(StickMode::Ragdoll);
		const BladeStickStep r = StepBladeStick(st, s, Frame(PoseAt(15.0f, 20.0f, 0.0f)));
		Check(r.end == StickEnd::None && r.drive, "aside 20: no tear for a hanging body (it follows)");
	}
	{
		BladeStickState st = Stuck(StickMode::Ragdoll);
		const BladeStickStep r = StepBladeStick(st, s, Frame(PoseAt(15.0f, 40.0f, 0.0f)));
		Check(r.end == StickEnd::TornOut && st.mode == StickMode::None,
		      "the body caught 40 units from the blade: torn out");
	}
	{
		BladeStickState st = Stuck(StickMode::Ragdoll);
		Check(StepBladeStick(st, s, Frame(PoseAt(8.0f))).end == StickEnd::PulledOut, "pulled back out of a body: free");
		BladeStickState st2 = Stuck(StickMode::Ragdoll);
		const BladeStickStep back = StepBladeStick(st2, s, Frame(PoseAt(12.0f)));
		Check(back.drive && NearP(back.shift, NiPoint3{0.0f, 0.0f, 0.0f}) && Near(st2.depth, 2.0f),
		      "drawn back along the line: the body is not pulled after it, the blade comes out of it");
	}
	{
		BladeStickState st = Stuck(StickMode::Standing);
		BecomeRagdoll(st);
		Check(st.mode == StickMode::Ragdoll, "killed with the blade in: now hanging on it");
		BladeStickState none;
		BecomeRagdoll(none);
		Check(none.mode == StickMode::None, "nothing stuck: a death changes nothing");
	}
}

}  // namespace

int main() {
	TestStart();
	TestStanding();
	TestBodyMovesUnderTheBlade();
	TestEnds();
	TestRagdoll();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
