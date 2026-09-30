// Checks the bow by hand (vr/Archery.h).

#include <cmath>
#include <cstdio>

#include "vr/Archery.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

const ArcherySettings kSettings;

// A bow drawn, the bow hand ahead at (-0.1, 1.4, -0.5); the drawing hand where
// asked, at the quiver or not.
ArcheryInput At(const NiPoint3& drawAt, bool grip, bool atQuiver = false) {
	ArcheryInput in;
	in.bowDrawn = true;
	in.drawValid = true;
	in.bowValid = true;
	in.bowAt = NiPoint3{-0.1f, 1.4f, -0.5f};
	in.drawAt = drawAt;
	in.drawBody = atQuiver ? kSettings.quiverZone : NiPoint3{0.2f, 0.3f, -0.3f};
	in.drawGrip = grip;
	return in;
}

const NiPoint3 kShoulder{0.15f, 1.5f, 0.1f};
const NiPoint3 kAtBow{-0.1f, 1.4f, -0.4f};    // 0.10 from the bow hand
const NiPoint3 kPulled{-0.1f, 1.4f, -0.2f};   // 0.30 from it
const NiPoint3 kBarely{-0.1f, 1.4f, -0.3f};   // 0.20: past the nock, short of the draw

void TestShot() {
	std::printf("An arrow from the quiver to the shot\n");
	ArcheryState st;
	ArcheryVerdict v = StepArchery(st, At(kShoulder, false, true), kSettings);
	Check(v.state == ArrowState::None && !v.claimsGrip, "the hand at the quiver, grip open: nothing");
	v = StepArchery(st, At(kShoulder, true, true), kSettings);
	Check(v.took && v.state == ArrowState::InHand && v.claimsGrip && !v.attackHeld && !v.aiming,
	      "the grip closes at the quiver: an arrow in the hand, the grip the arrow's");
	v = StepArchery(st, At(kPulled, true), kSettings);
	Check(v.state == ArrowState::InHand, "carried 0.30 from the bow: still in the hand");
	v = StepArchery(st, At(kAtBow, true), kSettings);
	Check(v.nocked && v.state == ArrowState::Nocked && v.aiming && !v.attackHeld,
	      "brought within 0.15 of the bow hand: nocked, aiming, not drawn yet");
	v = StepArchery(st, At(kBarely, true), kSettings);
	Check(v.state == ArrowState::Nocked && !v.attackHeld, "0.20 from it: still only nocked");
	v = StepArchery(st, At(kPulled, true), kSettings);
	Check(v.drawStarted && v.state == ArrowState::Drawing && v.attackHeld && v.aiming,
	      "pulled to 0.30: the draw begins, the attack held");
	v = StepArchery(st, At(kAtBow, true), kSettings);
	Check(v.state == ArrowState::Drawing && v.attackHeld, "eased back towards the bow: still drawn");
	v = StepArchery(st, At(kPulled, false), kSettings);
	Check(v.loosed && v.state == ArrowState::None && !v.attackHeld && !v.claimsGrip,
	      "the grip let go: loosed, the attack let go");
}

void TestDrops() {
	std::printf("Arrows that are not shot\n");
	ArcheryState st;
	StepArchery(st, At(kShoulder, false, true), kSettings);
	StepArchery(st, At(kShoulder, true, true), kSettings);
	ArcheryVerdict v = StepArchery(st, At(kPulled, false), kSettings);
	Check(v.dropped && v.state == ArrowState::None && !v.loosed, "let go before the bow: dropped");

	StepArchery(st, At(kShoulder, true, true), kSettings);
	StepArchery(st, At(kAtBow, true), kSettings);
	v = StepArchery(st, At(kAtBow, false), kSettings);
	Check(v.dropped && !v.loosed && !v.attackHeld, "let go nocked, never pulled: dropped, no shot");

	ArcheryState held;
	StepArchery(held, At(kShoulder, true, true), kSettings);
	v = StepArchery(held, At(kShoulder, true, true), kSettings);
	Check(!v.took && v.state == ArrowState::None, "a grip already closed when the bow came: no arrow");

	ArcheryState away;
	StepArchery(away, At(kShoulder, false), kSettings);
	v = StepArchery(away, At(kShoulder, true), kSettings);
	Check(!v.took && !v.claimsGrip, "the grip closed away from the quiver: no arrow, the grip free");
}

void TestGates() {
	std::printf("When there is no bow\n");
	ArcheryState st;
	StepArchery(st, At(kShoulder, false, true), kSettings);
	StepArchery(st, At(kShoulder, true, true), kSettings);
	StepArchery(st, At(kAtBow, true), kSettings);
	StepArchery(st, At(kPulled, true), kSettings);
	ArcheryInput sheathed = At(kPulled, true);
	sheathed.bowDrawn = false;
	ArcheryVerdict v = StepArchery(st, sheathed, kSettings);
	Check(v.dropped && !v.loosed && !v.attackHeld && v.state == ArrowState::None,
	      "the bow put away mid draw: the arrow gone, no shot");

	ArcherySettings off = kSettings;
	off.enabled = false;
	ArcheryState o;
	StepArchery(o, At(kShoulder, false, true), off);
	v = StepArchery(o, At(kShoulder, true, true), off);
	Check(!v.took && !v.claimsGrip, "switched off: the grip at the shoulder takes nothing");

	ArcheryState lost;
	StepArchery(lost, At(kShoulder, false, true), kSettings);
	StepArchery(lost, At(kShoulder, true, true), kSettings);
	ArcheryInput untracked = At(kShoulder, true);
	untracked.bowValid = false;
	v = StepArchery(lost, untracked, kSettings);
	Check(v.dropped && v.state == ArrowState::None, "the bow hand lost: the arrow gone");
}

void TestQuiver() {
	std::printf("The quiver's place\n");
	Check(AtQuiver(kSettings, kSettings.quiverZone, false), "at it");
	Check(!AtQuiver(kSettings, NiPoint3{-0.15f, -0.12f, -0.10f}, false), "the other shoulder: not");
	Check(AtQuiver(kSettings, NiPoint3{-0.15f, -0.12f, -0.10f}, true), "left-handed: the other shoulder");
	Check(!AtQuiver(kSettings, NiPoint3{0.15f, 0.4f, -0.10f}, false), "in front: not");
}

void TestLine() {
	std::printf("The arrow's line\n");
	NiPoint3 d{};
	Check(ArrowLine(NiPoint3{0, 1, 0}, NiPoint3{0, 1, -2}, d) && Near(d.z, -1.0f) && Near(d.x, 0.0f),
	      "from the drawing hand through the bow");
	Check(ArrowLine(NiPoint3{0.3f, 1, 0}, NiPoint3{0, 1, -0.4f}, d) && Near(d.x, -0.6f) && Near(d.z, -0.8f),
	      "the drawing hand to the side: the arrow points across");
	Check(!ArrowLine(NiPoint3{0, 1, 0}, NiPoint3{0, 1, 0}, d), "the hands in one place: no line");
}

}  // namespace

int main() {
	TestShot();
	TestDrops();
	TestGates();
	TestQuiver();
	TestLine();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
