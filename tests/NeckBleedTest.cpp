// A neck wound that bleeds on (game/NeckBleedLogic.h): which entries open
// one, its pulses, the most at once, and every way it ends - its pulses
// spent, the heart stopped after a death, a load, the actor gone, off.

#include <cstdio>

#include "game/NeckBleedLogic.h"

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

NeckWoundFrame Alive(UInt32 world = 7u, float dt = 0.1f) {
	NeckWoundFrame f;
	f.there = true;
	f.alive = true;
	f.worldNow = world;
	f.dtSeconds = dt;
	return f;
}

void TestOpens() {
	std::printf("which entries open a neck wound\n");
	NeckBleedSettings s;
	Check(NeckWoundOpens(s, true, BodyPart::Neck, BodyMaterial::Skin, false, false), "a blade into the bare neck: opens");
	Check(NeckWoundOpens(s, true, BodyPart::Neck, BodyMaterial::Cloth, false, false), "through a collar (cloth): opens");
	Check(!NeckWoundOpens(s, true, BodyPart::Neck, BodyMaterial::Light, false, false),
	      "light armour without a push through: none");
	Check(NeckWoundOpens(s, true, BodyPart::Neck, BodyMaterial::Light, true, false), "light armour pushed through: opens");
	Check(!NeckWoundOpens(s, true, BodyPart::Torso, BodyMaterial::Skin, false, false), "the torso: not the neck's wound");
	Check(!NeckWoundOpens(s, false, BodyPart::Neck, BodyMaterial::Skin, false, false), "a blade that did not go in: none");
	Check(!NeckWoundOpens(s, true, BodyPart::Neck, BodyMaterial::Skin, false, true),
	      "the head is off: the cut's own blood, not this");
	NeckBleedSettings off;
	off.enabled = false;
	Check(!NeckWoundOpens(off, true, BodyPart::Neck, BodyMaterial::Skin, false, false), "switched off: none");
}

void TestPulses() {
	std::printf("the pulses\n");
	NeckBleedSettings s;
	s.pulseSeconds = 1.0f;
	s.livingPulses = 3;
	NeckWound w;
	Check(OpenNeckWound(w, 0x10u, 7u), "opened");
	NeckWoundStep r = StepNeckWound(w, s, Alive(7u, 0.0f));
	Check(r.spurt && r.end == NeckWoundEnd::None && w.spurts == 1, "the first spurt at once");
	r = StepNeckWound(w, s, Alive(7u, 0.5f));
	Check(!r.spurt, "half a pulse on: none");
	r = StepNeckWound(w, s, Alive(7u, 0.6f));
	Check(r.spurt && w.spurts == 2, "a pulse on: the second");
	r = StepNeckWound(w, s, Alive(7u, 1.0f));
	Check(r.spurt && w.spurts == 3, "the third");
	r = StepNeckWound(w, s, Alive(7u, 1.0f));
	Check(!r.spurt && r.end == NeckWoundEnd::Spent && !w.open, "its pulses spent: it stops");
}

void TestDeath() {
	std::printf("the heart stops after a death\n");
	NeckBleedSettings s;
	s.pulseSeconds = 1.0f;
	s.livingPulses = 50;
	s.deadSeconds = 2.0f;
	NeckWound w;
	OpenNeckWound(w, 0x10u, 7u);
	StepNeckWound(w, s, Alive(7u, 0.0f));
	NeckWoundFrame dead = Alive(7u, 1.0f);
	dead.alive = false;
	NeckWoundStep r = StepNeckWound(w, s, dead);
	Check(r.spurt && r.end == NeckWoundEnd::None, "dead a second: still pumping");
	r = StepNeckWound(w, s, dead);
	Check(r.end == NeckWoundEnd::HeartStopped && !w.open, "dead two seconds: the heart stopped, no more");
	NeckWound c;
	OpenNeckWound(c, 0x10u, 7u, false);
	NeckWoundFrame corpse = Alive(7u, 0.0f);
	corpse.alive = false;
	r = StepNeckWound(c, s, corpse);
	Check(r.spurt, "a corpse's neck cut: one spurt");
	corpse.dtSeconds = 0.5f;
	r = StepNeckWound(c, s, corpse);
	Check(r.end == NeckWoundEnd::HeartStopped, "a corpse's wound stops at once after it (its heart stopped before)");
}

void TestOtherEnds() {
	std::printf("the other ends\n");
	NeckBleedSettings s;
	NeckWound w;
	OpenNeckWound(w, 0x10u, 7u);
	Check(StepNeckWound(w, s, Alive(8u)).end == NeckWoundEnd::Load && !w.open, "a load since: ended, unread");
	OpenNeckWound(w, 0x10u, 7u);
	NeckWoundFrame gone = Alive();
	gone.there = false;
	Check(StepNeckWound(w, s, gone).end == NeckWoundEnd::Gone, "the actor unloaded: ended");
	OpenNeckWound(w, 0x10u, 7u);
	NeckBleedSettings off;
	off.enabled = false;
	Check(StepNeckWound(w, off, Alive()).end == NeckWoundEnd::Off, "switched off: ended");
	NeckWound none;
	Check(StepNeckWound(none, s, Alive()).end == NeckWoundEnd::None && !StepNeckWound(none, s, Alive()).spurt,
	      "no wound: nothing");
	Check(!OpenNeckWound(none, 0u, 7u) && !none.open, "nobody: not opened");
}

void TestMostAtOnce() {
	std::printf("the most at once\n");
	NeckBleedSettings s;
	s.mostWounds = 2;
	NeckWounds ws;
	Check(AddNeckWound(ws, s, 0x10u, 7u, true) >= 0, "the first: room");
	Check(AddNeckWound(ws, s, 0x20u, 7u, true) >= 0, "the second: room");
	const int third = AddNeckWound(ws, s, 0x30u, 7u, true);
	Check(third >= 0 && ws.w[third].actor == 0x30u && OpenNeckWounds(ws) == 2,
	      "a third past the most: it takes the oldest's place");
	bool firstGone = true;
	for (UInt32 i = 0; i < kNeckWoundsMax; ++i) {
		firstGone = firstGone && !(ws.w[i].open && ws.w[i].actor == 0x10u);
	}
	Check(firstGone, "the oldest is the one dropped");
	Check(AddNeckWound(ws, s, 0x20u, 7u, true) >= 0 && OpenNeckWounds(ws) == 2,
	      "a second cut in the same neck: the wound opened anew, not a third");
}

}  // namespace

int main() {
	TestOpens();
	TestPulses();
	TestDeath();
	TestOtherEnds();
	TestMostAtOnce();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
