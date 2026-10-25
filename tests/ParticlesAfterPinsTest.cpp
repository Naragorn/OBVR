#include <cstdio>

#include "game/ParticlesAfterPins.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool ok, const char* what) {
	std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what);
	if (!ok) {
		++g_failures;
	}
}

ParticleDeferState Armed(UInt32 atMs) {
	ParticleDeferState s;
	DeferredParticleUpdate none[1];
	TakeDeferredParticleUpdates(s, none, 1, true, atMs);
	return s;
}

void TestRunsNow() {
	std::printf("Updates that run at once\n");
	ParticleDeferState s;
	Check(DecideParticleUpdate(s, 0x100, 1.0f, true, 0) == ParticleCall::Run, "the pins never ran: runs");
	Check(s.count == 0, "nothing queued");
	ParticleDeferState a = Armed(1000);
	Check(DecideParticleUpdate(a, 0x100, 1.0f, false, 1010) == ParticleCall::Run,
	      "outside the first-person tree: runs");
	Check(a.count == 0, "nothing queued for it");
	Check(DecideParticleUpdate(a, 0, 1.0f, true, 1010) == ParticleCall::Run, "no controller: runs");
	Check(DecideParticleUpdate(a, 0x100, 1.0f, true, 1000 + kParticleDeferArmedMs + 1) == ParticleCall::Run,
	      "the pins last ran too long ago: runs");
}

void TestHeldBack() {
	std::printf("Updates held back for the pins\n");
	ParticleDeferState s = Armed(1000);
	Check(DecideParticleUpdate(s, 0x100, 13.4f, true, 1011) == ParticleCall::Defer, "the fire's update waits");
	Check(s.count == 1 && s.queued[0].controller == 0x100 && s.queued[0].time == 13.4f, "queued with its time");
	Check(DecideParticleUpdate(s, 0x100, 13.5f, true, 1012) == ParticleCall::Defer,
	      "the same controller again waits");
	Check(s.count == 1 && s.queued[0].time == 13.5f, "one entry, the latest time");
	Check(DecideParticleUpdate(s, 0x200, 13.5f, true, 1012) == ParticleCall::Defer, "the sparks wait too");
	Check(s.count == 2 && s.queued[1].controller == 0x200, "queued second");
	Check(DecideParticleUpdate(s, 0x300, 13.5f, true, 1000 + kParticleDeferArmedMs) == ParticleCall::Defer,
	      "at the edge of the armed time it still waits");
}

void TestFull() {
	std::printf("A full queue\n");
	ParticleDeferState s = Armed(0);
	for (UInt32 i = 0; i < kParticleDeferMost; ++i) {
		DecideParticleUpdate(s, 0x100 + i, 1.0f, true, 5);
	}
	Check(s.count == kParticleDeferMost, "filled");
	Check(DecideParticleUpdate(s, 0x900, 1.0f, true, 5) == ParticleCall::Run, "one more runs at once");
	Check(DecideParticleUpdate(s, 0x100, 2.0f, true, 5) == ParticleCall::Defer, "a queued one still waits");
}

void TestTake() {
	std::printf("After the pins\n");
	ParticleDeferState s = Armed(1000);
	DecideParticleUpdate(s, 0x100, 13.4f, true, 1005);
	DecideParticleUpdate(s, 0x200, 13.4f, true, 1005);
	DeferredParticleUpdate out[kParticleDeferMost];
	const UInt32 taken = TakeDeferredParticleUpdates(s, out, kParticleDeferMost, true, 1012);
	Check(taken == 2 && out[0].controller == 0x100 && out[1].controller == 0x200, "both, in order");
	Check(out[0].time == 13.4f, "with their times");
	Check(s.count == 0, "the queue empty");
	Check(ParticleDeferArmed(s, 1020), "armed for the next frame");
	DecideParticleUpdate(s, 0x100, 13.5f, true, 1020);
	DecideParticleUpdate(s, 0x200, 13.5f, true, 1020);
	DeferredParticleUpdate one[1];
	Check(TakeDeferredParticleUpdates(s, one, 1, true, 1025) == 1, "no more than the room given");
	Check(s.count == 0, "the rest dropped with the queue");
}

void TestDisarm() {
	std::printf("A frame without the pins\n");
	ParticleDeferState s = Armed(1000);
	DecideParticleUpdate(s, 0x100, 13.4f, true, 1005);
	DeferredParticleUpdate out[kParticleDeferMost];
	Check(TakeDeferredParticleUpdates(s, out, kParticleDeferMost, false, 1012) == 1,
	      "what waited runs, at the game's own pose");
	Check(!ParticleDeferArmed(s, 1013), "disarmed");
	Check(DecideParticleUpdate(s, 0x100, 13.5f, true, 1020) == ParticleCall::Run, "the next update runs at once");
	ParticleDeferState t = Armed(1000);
	DecideParticleUpdate(t, 0x100, 13.4f, true, 1005);
	Check(DecideParticleUpdate(t, 0x100, 13.9f, true, 1300) == ParticleCall::Run,
	      "the pins stopped without a take: runs");
	Check(t.count == 0, "and its stale entry is dropped");
}

void TestWrap() {
	std::printf("The millisecond clock wrapping\n");
	ParticleDeferState s = Armed(0xFFFFFFF0u);
	Check(DecideParticleUpdate(s, 0x100, 1.0f, true, 0x00000010u) == ParticleCall::Defer,
	      "32 ms on across the wrap: still armed");
}

}  // namespace

int main() {
	TestRunsNow();
	TestHeldBack();
	TestFull();
	TestTake();
	TestDisarm();
	TestWrap();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
