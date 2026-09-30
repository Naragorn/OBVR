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

void TestStringWeight() {
	std::printf("The string's weight from the nock\n");
	Check(Near(StringWeightAt(16.0f, 16.0f, 28.0f), 0.0f), "the nock where the string rests: 0");
	Check(Near(StringWeightAt(30.0f, 16.0f, 28.0f), 0.5f), "half the travel back: 0.5");
	Check(Near(StringWeightAt(44.0f, 16.0f, 28.0f), 1.0f), "the full travel: 1");
	Check(Near(StringWeightAt(60.0f, 16.0f, 28.0f), 1.0f), "further than the string goes: held at 1");
	Check(Near(StringWeightAt(5.0f, 16.0f, 28.0f), 0.0f), "nearer the bow than the string: 0");
	Check(Near(StringWeightAt(30.0f, 16.0f, 0.0f), 0.0f), "a bow with no travel: 0");
}

void TestStringSource() {
	std::printf("Who has the string\n");
	BowStringState s;
	Check(StepBowString(s, ArrowState::None, false, false, 0.01f) == StringSource::Engine, "no arrow: the game's");
	Check(StepBowString(s, ArrowState::InHand, false, false, 0.01f) == StringSource::Engine,
	      "an arrow in the hand, not on the string: the game's");
	Check(StepBowString(s, ArrowState::Nocked, false, false, 0.01f) == StringSource::Hand, "nocked: the hand's");
	Check(StepBowString(s, ArrowState::Drawing, false, true, 0.01f) == StringSource::Hand, "drawn: the hand's");
	Check(StepBowString(s, ArrowState::None, true, true, 0.01f) == StringSource::Rest, "loosed: at rest");
	Check(StepBowString(s, ArrowState::None, false, true, 0.5f) == StringSource::Rest,
	      "the engine's shot still playing: held at rest");
	Check(StepBowString(s, ArrowState::None, false, false, 0.01f) == StringSource::Engine,
	      "the shot over (action -1): the game's again");
	Check(StepBowString(s, ArrowState::None, false, true, 0.01f) == StringSource::Engine,
	      "the next action is the game's own");

	BowStringState slow;
	StepBowString(slow, ArrowState::Drawing, false, true, 0.01f);
	StepBowString(slow, ArrowState::None, true, true, 0.01f);
	Check(StepBowString(slow, ArrowState::None, false, true, 2.0f) == StringSource::Rest, "2 s on: still at rest");
	Check(StepBowString(slow, ArrowState::None, false, true, 1.5f) == StringSource::Engine,
	      "past 3 s with the action still on: given back anyway");

	BowStringState dropped;
	StepBowString(dropped, ArrowState::Nocked, false, false, 0.01f);
	Check(StepBowString(dropped, ArrowState::None, false, false, 0.01f) == StringSource::Rest,
	      "dropped from the string: back to rest once");
	Check(StepBowString(dropped, ArrowState::None, false, false, 0.01f) == StringSource::Engine,
	      "then the game's");

	BowStringState again;
	StepBowString(again, ArrowState::Drawing, false, true, 0.01f);
	StepBowString(again, ArrowState::None, true, true, 0.01f);
	Check(StepBowString(again, ArrowState::Nocked, false, true, 0.01f) == StringSource::Hand,
	      "the next arrow nocked while the last shot plays: the hand's");
}

void TestArrowShown() {
	std::printf("Where the arrow is seen\n");
	Check(ArrowShownFor(ArrowState::None) == ArrowShown::None, "no arrow: none");
	Check(ArrowShownFor(ArrowState::InHand) == ArrowShown::InHand, "taken: in the fist");
	Check(ArrowShownFor(ArrowState::Nocked) == ArrowShown::OnString, "nocked: on the string");
	Check(ArrowShownFor(ArrowState::Drawing) == ArrowShown::OnString, "drawn: on the string");
}

bool NearPoint(const NiPoint3& a, const NiPoint3& b) { return Near(a.x, b.x) && Near(a.y, b.y) && Near(a.z, b.z); }

bool Orthonormal(const NiMatrix33& m) {
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			float dot = 0.0f;
			for (int k = 0; k < 3; ++k) {
				dot += m.data[k][i] * m.data[k][j];
			}
			if (!Near(dot, i == j ? 1.0f : 0.0f)) {
				return false;
			}
		}
	}
	return true;
}

void TestTurn() {
	std::printf("Turning the arrow onto its line\n");
	const NiMatrix33 id = NiMatrix33::Identity();
	NiMatrix33 r = TurnYOnto(id, NiPoint3{0, 1, 0});
	Check(NearPoint(r * NiPoint3{1, 0, 0}, NiPoint3{1, 0, 0}) && NearPoint(r * NiPoint3{0, 1, 0}, NiPoint3{0, 1, 0}),
	      "already along it: unchanged");
	r = TurnYOnto(id, NiPoint3{0, 0, 1});
	Check(NearPoint(r * NiPoint3{0, 1, 0}, NiPoint3{0, 0, 1}) && NearPoint(r * NiPoint3{1, 0, 0}, NiPoint3{1, 0, 0}) &&
	          Orthonormal(r),
	      "a quarter turn up: y onto z, x (the roll) kept");
	r = TurnYOnto(id, NiPoint3{0, -1, 0});
	Check(NearPoint(r * NiPoint3{0, 1, 0}, NiPoint3{0, -1, 0}) && NearPoint(r * NiPoint3{1, 0, 0}, NiPoint3{1, 0, 0}) &&
	          Orthonormal(r),
	      "straight back: half a turn about x");
	const float s = 0.70710678f;
	NiMatrix33 rolled = id;  // turned 90 degrees about y: x onto -z
	rolled.data[0][0] = 0.0f;
	rolled.data[0][2] = 1.0f;
	rolled.data[2][0] = -1.0f;
	rolled.data[2][2] = 0.0f;
	r = TurnYOnto(rolled, NiPoint3{s, s, 0});
	Check(NearPoint(r * NiPoint3{0, 1, 0}, NiPoint3{s, s, 0}) && Orthonormal(r) &&
	          NearPoint(r * NiPoint3{1, 0, 0}, NiPoint3{0, 0, -1}),
	      "a rolled fist turned sideways: y onto the line, the roll's axis kept off it");
}

void TestArrowPose() {
	std::printf("The arrow's pose\n");
	const NiMatrix33 id = NiMatrix33::Identity();
	const NiPoint3 grip{10, 0, 0};
	const NiPoint3 rest{10, 40, 0};
	ArrowPose p;
	Check(!ArrowPoseFor(ArrowShown::None, id, grip, rest, 46.6f, p), "no arrow: no pose");
	Check(!ArrowPoseFor(ArrowShown::InHand, id, grip, rest, 0.0f, p), "a model with no length: no pose");
	Check(ArrowPoseFor(ArrowShown::InHand, id, grip, rest, 46.6f, p) &&
	          NearPoint(p.nock, NiPoint3{10, -kArrowNockBehindGripUnits, 0}) &&
	          NearPoint(p.pos, NiPoint3{10, 46.6f - kArrowNockBehindGripUnits, 0}),
	      "in the fist: the nock behind the grip, the head ahead along it");
	const NiPoint3 aside{10 + 43.6f, -kArrowNockBehindGripUnits, 0};
	Check(ArrowPoseFor(ArrowShown::OnString, id, grip, aside, 46.6f, p) &&
	          NearPoint(p.pos, NiPoint3{10 + 46.6f, -kArrowNockBehindGripUnits, 0}) &&
	          NearPoint(p.rot * NiPoint3{0, 1, 0}, NiPoint3{1, 0, 0}),
	      "on the string: from the nock through the bow's rest, the head past it");
	const NiPoint3 onNock{10, -kArrowNockBehindGripUnits, 0};
	Check(!ArrowPoseFor(ArrowShown::OnString, id, grip, onNock, 46.6f, p), "the rest on the nock: no pose");
}

}  // namespace

int main() {
	TestShot();
	TestDrops();
	TestGates();
	TestQuiver();
	TestLine();
	TestStringWeight();
	TestStringSource();
	TestArrowShown();
	TestTurn();
	TestArrowPose();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
