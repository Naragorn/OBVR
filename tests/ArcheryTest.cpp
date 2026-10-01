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
	v = StepArchery(st, At(kBarely, true), kSettings);
	Check(v.state == ArrowState::Drawing && v.attackHeld, "eased to 0.20: still drawn");
	v = StepArchery(st, At(kPulled, false), kSettings);
	Check(v.loosed && v.state == ArrowState::None && !v.attackHeld && !v.claimsGrip,
	      "the grip let go: loosed, the attack let go");
}

// The bow pointing along -z from (-0.1, 1.4, -0.5), the drawing hand where
// asked: pulled back along +z, or off to the side.
ArcheryInput OnAxis(const NiPoint3& drawAt, bool hold, float dt = 0.011f) {
	ArcheryInput in = At(drawAt, hold);
	in.axisValid = true;
	in.bowAxis = NiPoint3{0.0f, 0.0f, -1.0f};
	in.dtSeconds = dt;
	return in;
}

void TestEase() {
	std::printf("Easing the draw, taking the arrow off, putting it back\n");
	ArcheryState st;
	StepArchery(st, At(kShoulder, false, true), kSettings);
	StepArchery(st, At(kShoulder, true, true), kSettings);
	StepArchery(st, OnAxis(kAtBow, true), kSettings);
	ArcheryVerdict v = StepArchery(st, OnAxis(kPulled, true), kSettings);
	Check(v.state == ArrowState::Drawing && Near(v.pullMetres, 0.30f) && Near(v.offLineMetres, 0.0f),
	      "pulled 0.30 straight back along the bow: drawn");
	v = StepArchery(st, OnAxis(NiPoint3{0.2f, 1.4f, -0.2f}, true), kSettings);
	Check(v.state == ArrowState::Drawing && Near(v.pullMetres, 0.30f) && Near(v.offLineMetres, 0.30f),
	      "the hand swung 0.30 aside: still the same pull along the bow, still drawn");
	v = StepArchery(st, OnAxis(kAtBow, true), kSettings);
	Check(v.eased && v.state == ArrowState::InHand && v.attackHeld && !v.aiming && !v.loosed,
	      "brought back to the bow: eased, the arrow back in the hand, the control still held for the cancel");
	v = StepArchery(st, OnAxis(kAtBow, true, 0.011f), kSettings);
	Check(v.state == ArrowState::InHand && !v.nocked, "still at the bow: not nocked again at once");
	bool done = false;
	for (int i = 0; i < 40 && v.attackHeld; ++i) {
		v = StepArchery(st, OnAxis(kAtBow, true, 0.011f), kSettings);
		done = done || v.denockDone;
	}
	Check(done && !v.attackHeld && v.state == ArrowState::InHand,
	      "0.3 s on: the control let go (the cancel's end), the arrow still in the hand");
	v = StepArchery(st, OnAxis(NiPoint3{-0.1f, 1.4f, -0.2f}, true), kSettings);
	Check(v.state == ArrowState::InHand && !v.drawStarted, "pulled back from the bow in the hand: no draw");
	v = StepArchery(st, OnAxis(kAtBow, true), kSettings);
	Check(v.nocked && v.state == ArrowState::Nocked, "out of the nock's reach and back: nocked again");
	v = StepArchery(st, OnAxis(kPulled, true), kSettings);
	Check(v.drawStarted, "and it can be drawn again");
	StepArchery(st, OnAxis(kAtBow, true, 0.5f), kSettings);
	v = StepArchery(st, OnAxis(kAtBow, true, 0.5f), kSettings);
	Check(v.denockDone && !v.attackHeld, "a long frame ends the cancel at once");
	ArcheryInput atQuiver = OnAxis(kShoulder, false);
	atQuiver.drawBody = kSettings.quiverZone;
	v = StepArchery(st, atQuiver, kSettings);
	Check(v.stowed && !v.dropped && v.state == ArrowState::None, "the eased arrow let go at the quiver: put back");

	ArcheryState off;
	StepArchery(off, At(kShoulder, false, true), kSettings);
	StepArchery(off, At(kShoulder, true, true), kSettings);
	StepArchery(off, OnAxis(kAtBow, true), kSettings);
	v = StepArchery(off, OnAxis(NiPoint3{-0.02f, 1.4f, -0.4f}, true), kSettings);
	Check(v.state == ArrowState::Nocked && !v.unnocked && Near(v.offLineMetres, 0.08f),
	      "nocked, the hand 0.08 off the bow's line: still on the string");
	v = StepArchery(off, OnAxis(NiPoint3{0.06f, 1.4f, -0.4f}, true), kSettings);
	Check(v.unnocked && v.state == ArrowState::InHand && !v.aiming,
	      "0.16 off the line, out of the nock's reach: off the string, in the hand");
	v = StepArchery(off, OnAxis(kAtBow, true), kSettings);
	Check(v.nocked, "taken off by moving away: nocked again at once when brought back");

	ArcheryState away;
	StepArchery(away, At(kShoulder, false, true), kSettings);
	StepArchery(away, At(kShoulder, true, true), kSettings);
	StepArchery(away, OnAxis(kAtBow, true), kSettings);
	v = StepArchery(away, OnAxis(NiPoint3{-0.1f, 1.4f, -0.44f}, true), kSettings);
	Check(v.state == ArrowState::Nocked && !v.unnocked, "a hand wobbling on the line stays nocked");
	ArcheryInput noAxis = At(NiPoint3{0.3f, 1.4f, -0.5f}, true);
	ArcheryState plain;
	StepArchery(plain, At(kShoulder, false, true), kSettings);
	StepArchery(plain, At(kShoulder, true, true), kSettings);
	StepArchery(plain, At(kAtBow, true), kSettings);
	v = StepArchery(plain, noAxis, kSettings);
	Check(v.drawStarted && !v.unnocked, "no bow axis: 0.40 away in any direction is a draw, as before");

	ArcheryState gone;
	StepArchery(gone, At(kShoulder, false, true), kSettings);
	StepArchery(gone, At(kShoulder, true, true), kSettings);
	StepArchery(gone, OnAxis(kAtBow, true), kSettings);
	StepArchery(gone, OnAxis(kPulled, true), kSettings);
	StepArchery(gone, OnAxis(kAtBow, true), kSettings);
	ArcheryInput sheathed = OnAxis(kAtBow, true);
	sheathed.bowDrawn = false;
	v = StepArchery(gone, sheathed, kSettings);
	Check(v.denockDone && !v.attackHeld && v.dropped, "the bow put away mid-cancel: the cancel ended with it");
}

// The nock is found around where the string rests, 0.15 behind the bow hand
// (the tester, 2026-10-01: "näher an die position"); the draw begins and
// eases from there.
void TestWideNock() {
	std::printf("The nock at the string's place\n");
	ArcheryState near;
	StepArchery(near, At(kShoulder, false, true), kSettings);
	StepArchery(near, At(kShoulder, true, true), kSettings);
	ArcheryVerdict v = StepArchery(near, OnAxis(NiPoint3{-0.1f, 1.4f, -0.47f}, true), kSettings);
	Check(!v.nocked && Near(v.fromNockMetres, 0.12f), "right at the bow hand, 0.12 before the string's place: not yet");
	ArcheryState st;
	StepArchery(st, At(kShoulder, false, true), kSettings);
	StepArchery(st, At(kShoulder, true, true), kSettings);
	v = StepArchery(st, OnAxis(NiPoint3{-0.02f, 1.4f, -0.33f}, true), kSettings);
	Check(v.nocked && v.fromNockMetres > 0.08f && v.fromNockMetres < 0.09f,
	      "within 0.09 of where the string rests, behind the bow and a little aside: nocked");
	v = StepArchery(st, OnAxis(NiPoint3{0.0f, 1.4f, -0.29f}, true), kSettings);
	Check(v.state == ArrowState::Nocked && !v.drawStarted, "0.21 behind the bow: short of the draw (0.15 + 0.08)");
	v = StepArchery(st, OnAxis(NiPoint3{-0.1f, 1.4f, -0.26f}, true), kSettings);
	Check(v.drawStarted, "0.24 behind it: drawn, no later than with the old nock");
	v = StepArchery(st, OnAxis(NiPoint3{-0.1f, 1.4f, -0.34f}, true), kSettings);
	Check(v.state == ArrowState::Drawing, "0.16 behind: still drawn");
	v = StepArchery(st, OnAxis(NiPoint3{-0.1f, 1.4f, -0.36f}, true), kSettings);
	Check(v.eased, "0.14 behind, at the string's rest: eased");
	ArcheryState far;
	StepArchery(far, At(kShoulder, false, true), kSettings);
	StepArchery(far, At(kShoulder, true, true), kSettings);
	v = StepArchery(far, OnAxis(NiPoint3{-0.1f, 1.4f, -0.23f}, true), kSettings);
	Check(!v.nocked && v.state == ArrowState::InHand && v.fromNockMetres > 0.1f,
	      "0.27 behind the bow hand, 0.12 past the string's place: not yet");

	// Only from behind (the tester, 2026-10-01: "von vorne kommen oder andere
	// seiten soll natürlich nicht gehen").
	v = StepArchery(far, OnAxis(NiPoint3{-0.1f, 1.4f, -0.6f}, true), kSettings);
	Check(!v.nocked && v.pullMetres < 0.0f, "0.10 in front of the bow: not nocked");
	v = StepArchery(far, OnAxis(NiPoint3{0.05f, 1.4f, -0.5f}, true), kSettings);
	Check(!v.nocked, "0.15 to its side, level with it: not nocked");
	v = StepArchery(far, OnAxis(NiPoint3{0.08f, 1.4f, -0.44f}, true), kSettings);
	Check(!v.nocked && v.offLineMetres > v.pullMetres, "behind, but more aside than behind: not nocked");
	v = StepArchery(far, OnAxis(NiPoint3{-0.1f, 1.4f, -0.49f}, true), kSettings);
	Check(!v.nocked, "a centimetre behind it: not yet");
	v = StepArchery(far, OnAxis(NiPoint3{-0.05f, 1.4f, -0.42f}, true), kSettings);
	Check(v.nocked, "then brought round behind it: nocked");
	ArcheryState plain;
	StepArchery(plain, At(kShoulder, false, true), kSettings);
	StepArchery(plain, At(kShoulder, true, true), kSettings);
	v = StepArchery(plain, At(NiPoint3{-0.1f, 1.4f, -0.58f}, true), kSettings);
	Check(v.nocked, "no bow axis: any side of the bow hand, as before");
	Check(FromBehind(true, 0.1f, 0.1f) && !FromBehind(true, 0.1f, 0.11f) && !FromBehind(true, 0.01f, 0.0f) &&
	          FromBehind(false, -1.0f, 5.0f),
	      "the cone's edges: 45 degrees, two centimetres; no axis, anything");
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

	ArcheryState back;
	StepArchery(back, At(kShoulder, false, true), kSettings);
	StepArchery(back, At(kShoulder, true, true), kSettings);
	v = StepArchery(back, At(kShoulder, false, true), kSettings);
	Check(v.stowed && !v.dropped, "taken and let go at the quiver again: put back, not dropped");
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
	ArrowPose p;
	Check(ArrowAlong(NiPoint3{1, 2, 3}, NiPoint3{0, 0, 2}, id, 46.6f, p) && NearPoint(p.nock, NiPoint3{1, 2, 3}) &&
	          NearPoint(p.pos, NiPoint3{1, 2, 49.6f}) && NearPoint(p.rot * NiPoint3{0, 1, 0}, NiPoint3{0, 0, 1}),
	      "along a line: the nock where given, the head ahead along it");
	Check(!ArrowAlong(NiPoint3{1, 2, 3}, NiPoint3{0, 0, 1}, id, 0.0f, p), "a model with no length: no pose");
	Check(!ArrowAlong(NiPoint3{1, 2, 3}, NiPoint3{0, 0, 0}, id, 46.6f, p), "no direction: no pose");

	// The fist: the wrist at the origin, the knuckle 8 units ahead and a little
	// up, the grip point in between and below.
	const NiPoint3 grip{0, 4, -1};
	const NiPoint3 wrist{0, 0, 0};
	const NiPoint3 knuckle{0, 8, 0.8f};
	// The fist's grip axis (its y) up the world's z, the hand along y: the
	// arrow 15 degrees below the hand's line.
	NiMatrix33 fist = NiMatrix33::Identity();
	fist.data[1][1] = 0.0f;
	fist.data[2][1] = 1.0f;
	fist.data[1][2] = -1.0f;
	fist.data[2][2] = 0.0f;
	const NiPoint3 flatKnuckle{0, 8, 0};
	Check(ArrowInFist(grip, wrist, flatKnuckle, fist, 46.6f, p), "in the fist: posed");
	const float c = std::cos(15.0f * 3.14159265f / 180.0f);
	const float sn = std::sin(15.0f * 3.14159265f / 180.0f);
	const NiPoint3 ahead{0, c, -sn};
	Check(NearPoint(p.rot * NiPoint3{0, 1, 0}, ahead), "along the hand, wrist to knuckle, 15 degrees down from it");
	Check(NearPoint(p.nock, grip - ahead * kArrowNockBehindGripUnits) && NearPoint(p.pos, p.nock + ahead * 46.6f),
	      "through the fist's middle: the nock just behind its grip point, the head ahead");
	// The hand tilted up a little: "down" is still away from the grip axis,
	// square to the hand's line.
	Check(ArrowInFist(grip, wrist, knuckle, fist, 46.6f, p), "a tilted hand: posed");
	const float l = std::sqrt(64.0f + 0.64f);
	const NiPoint3 unit{0, 8 / l, 0.8f / l};
	const NiPoint3 up{0, -0.8f / l, 8 / l};
	Check(NearPoint(p.rot * NiPoint3{0, 1, 0}, unit * c - up * sn), "down from the hand's own line, not the world's");
	Check(!ArrowInFist(grip, wrist, wrist, fist, 46.6f, p), "the knuckle on the wrist: no pose");
	Check(!ArrowInFist(grip, wrist, NiPoint3{0, 0, 8}, fist, 46.6f, p), "the grip axis along the hand: no pose");
	Check(!ArrowInFist(grip, wrist, knuckle, fist, 0.0f, p), "a model with no length: no pose");

	// The bow's axis along +y, its arrow rest at (0, 100, 0); the string rests
	// 15.6 behind, a full draw 28 more.
	const NiPoint3 rest{0, 100, 0};
	const NiPoint3 axis{0, 1, 0};
	ArrowOnString a;
	Check(!ArrowOnBowLine(rest, axis, id, NiPoint3{0, 80, 0}, 0.0f, 15.6f, 28.0f, a), "no length: no pose");
	Check(ArrowOnBowLine(rest, axis, id, NiPoint3{0, 95, 0}, 46.6f, 15.6f, 28.0f, a) &&
	          Near(a.nockBehindRest, 15.6f) && Near(a.weight, 0.0f) && !a.atFullDraw &&
	          NearPoint(a.gripTarget, NiPoint3{0, 100 - 15.6f + kArrowNockBehindGripUnits, 0}),
	      "the fist at the bow: the nock on the string at rest, the fist to go there");
	Check(ArrowOnBowLine(rest, axis, id, NiPoint3{3, 70, -2}, 46.6f, 15.6f, 28.0f, a) &&
	          Near(a.nockBehindRest, 30.0f + kArrowNockBehindGripUnits) &&
	          NearPoint(a.pose.nock, NiPoint3{0, 100 - 30 - kArrowNockBehindGripUnits, 0}) &&
	          NearPoint(a.pose.pos, a.pose.nock + axis * 46.6f) && !a.atFullDraw &&
	          NearPoint(a.pose.rot * NiPoint3{0, 1, 0}, axis),
	      "pulled 30 back and a little aside: on the line, as far back as the fist, along the bow");
	Check(Near(a.weight, (33.0f - 15.6f) / 28.0f), "the string's weight from the pull");
	Check(ArrowOnBowLine(rest, axis, id, NiPoint3{0, 20, 0}, 46.6f, 15.6f, 28.0f, a) &&
	          Near(a.nockBehindRest, 43.6f) && Near(a.weight, 1.0f) && a.atFullDraw &&
	          NearPoint(a.gripTarget, NiPoint3{0, 100 - 43.6f + kArrowNockBehindGripUnits, 0}),
	      "pulled past a full draw: stopped there, the fist held at it");
}

void TestBowSounds() {
	std::printf("The draw sound's parts\n");
	ArcheryVerdict v;
	v.state = ArrowState::Nocked;
	v.nocked = true;
	BowSoundCue c = BowSoundsFor(v);
	Check(c.nock && !c.stretch && c.stopStretch, "nocked: the first part, no stretch");
	v = ArcheryVerdict{};
	v.state = ArrowState::Drawing;
	v.drawStarted = true;
	c = BowSoundsFor(v);
	Check(!c.nock && c.stretch && !c.stopStretch, "the draw begins: the stretch");
	v.drawStarted = false;
	c = BowSoundsFor(v);
	Check(!c.nock && !c.stretch && !c.stopStretch, "drawing on: the stretch left to play");
	const ArrowState notDrawn[] = {ArrowState::None, ArrowState::InHand, ArrowState::Nocked};
	for (ArrowState s : notDrawn) {
		v = ArcheryVerdict{};
		v.state = s;
		c = BowSoundsFor(v);
		Check(c.stopStretch && !c.stretch && !c.nock, "not drawn (loosed, dropped, off the string, eased): cut off");
	}
}

void TestReleaseSnap() {
	std::printf("The loose at once\n");
	ReleaseSnapState s;
	Check(StepReleaseSnap(s, false, 5, false, 0.011f) == ReleaseSnap::Idle, "drawn, not loosed: nothing");
	Check(StepReleaseSnap(s, true, 5, false, 0.011f) == ReleaseSnap::Wait,
	      "the loose's own frame: wait for the control to be up");
	Check(StepReleaseSnap(s, false, 5, false, 0.011f) == ReleaseSnap::ToHold,
	      "the next, the arrow attached: to the Hold");
	Check(StepReleaseSnap(s, false, 5, true, 0.011f) == ReleaseSnap::Idle, "done: nothing more");

	// Legolas: let go before the Attach.
	ReleaseSnapState early;
	Check(StepReleaseSnap(early, true, 4, false, 0.011f) == ReleaseSnap::Wait, "let go before the Attach: its frame waits");
	Check(StepReleaseSnap(early, false, 4, false, 0.011f) == ReleaseSnap::ToAttach,
	      "the next: to the Attach first - one key at a time");
	Check(StepReleaseSnap(early, false, 4, false, 0.011f) == ReleaseSnap::ToAttach, "asked again until it is on");
	Check(StepReleaseSnap(early, false, 5, false, 0.011f) == ReleaseSnap::ToHold, "attached: to the Hold");

	// A draw of a frame: the engine's draw not seen yet when let go.
	ReleaseSnapState flick;
	Check(StepReleaseSnap(flick, true, -1, false, 0.011f) == ReleaseSnap::Wait, "let go before the draw shows: wait");
	Check(StepReleaseSnap(flick, false, -1, false, 0.011f) == ReleaseSnap::Wait, "still not shown: wait");
	Check(StepReleaseSnap(flick, false, 4, false, 0.011f) == ReleaseSnap::ToAttach, "it shows: to the Attach");
	ReleaseSnapState none;
	StepReleaseSnap(none, true, -1, false, 0.011f);
	Check(StepReleaseSnap(none, false, -1, false, 0.3f) == ReleaseSnap::Idle,
	      "no draw a quarter second on: given up");

	ReleaseSnapState shot;
	StepReleaseSnap(shot, true, 5, false, 0.011f);
	Check(StepReleaseSnap(shot, false, 3, false, 0.011f) == ReleaseSnap::Idle, "already loosed (3): nothing");
	Check(StepReleaseSnap(shot, false, 5, false, 0.011f) == ReleaseSnap::Idle, "and not again");
	ReleaseSnapState cancelled;
	StepReleaseSnap(cancelled, true, 4, false, 0.011f);
	Check(StepReleaseSnap(cancelled, false, -1, false, 0.011f) == ReleaseSnap::Idle,
	      "the draw seen, then gone (-1): nothing");
	ReleaseSnapState stuck;
	StepReleaseSnap(stuck, true, 4, false, 0.011f);
	Check(StepReleaseSnap(stuck, false, 4, false, 1.0f) == ReleaseSnap::ToAttach, "a second on: still asked");
	Check(StepReleaseSnap(stuck, false, 4, false, 1.1f) == ReleaseSnap::Idle, "past two seconds: given up");
	ReleaseSnapState unreadable;
	StepReleaseSnap(unreadable, true, 5, false, 0.011f);
	StepReleaseSnap(unreadable, false, 5, false, 0.011f);
	Check(StepReleaseSnap(unreadable, false, 5, false, 0.011f) == ReleaseSnap::ToHold,
	      "a snap not carried out is asked again");
}

void TestBowPower() {
	std::printf("The power from the draw\n");
	const auto power = [](float timer) { return std::fmin(1.0f, 0.25f + 0.4f * timer); };
	Check(Near(power(BowTimerForDraw(0.0f, 0.25f, 0.4f)), 0.25f), "a string at rest: vanilla's least, 0.25");
	Check(Near(power(BowTimerForDraw(1.0f, 0.25f, 0.4f)), 1.0f), "a full draw: full power");
	Check(Near(power(BowTimerForDraw(0.5f, 0.25f, 0.4f)), 0.625f), "half drawn: halfway between");
	Check(Near(BowTimerForDraw(2.0f, 0.25f, 0.4f), BowTimerForDraw(1.0f, 0.25f, 0.4f)) &&
	          Near(BowTimerForDraw(-1.0f, 0.25f, 0.4f), 0.0f),
	      "past either end: held to it");
	Check(Near(BowTimerForDraw(1.0f, 0.25f, 0.0f), 0.0f), "settings that never grow: nothing written but 0");
	Check(Near(BowTimerForDraw(1.0f, 2.0f, 0.4f), 0.0f) && Near(BowTimerForDraw(1.0f, -1.0f, 0.4f), 2.5f),
	      "a base past 1 or below 0: held to the range");

	BowPowerState s;
	float w = -1.0f;
	Check(!StepBowPower(s, false, true, 0.3f, false, -1, w), "nothing drawn: nothing written");
	Check(StepBowPower(s, true, true, 0.4f, false, 4, w) && Near(w, 0.4f), "drawn: the string's weight");
	Check(StepBowPower(s, true, false, 0.9f, false, 5, w) && Near(w, 0.4f), "a frame unread: the last weight kept");
	StepBowPower(s, true, true, 0.8f, false, 5, w);
	Check(StepBowPower(s, false, false, 0.0f, true, 5, w) && Near(w, 0.8f), "let go: the weight it had");
	Check(StepBowPower(s, false, false, 0.0f, false, 5, w) && Near(w, 0.8f), "until the engine looses");
	Check(!StepBowPower(s, false, false, 0.0f, false, 3, w), "loosed (3): no more");
	Check(!StepBowPower(s, false, false, 0.0f, false, 5, w), "and not again");

	BowPowerState unread;
	Check(!StepBowPower(unread, true, false, 0.0f, false, 4, w), "a draw never read: the game's own timer");
	Check(!StepBowPower(unread, false, false, 0.0f, true, 5, w), "and its loose too");
	BowPowerState next;
	StepBowPower(next, true, true, 0.7f, false, 5, w);
	StepBowPower(next, false, false, 0.0f, false, 12, w);
	Check(!StepBowPower(next, true, false, 0.0f, false, 4, w), "an eased draw's weight is not the next draw's");
}

void TestNoArrows() {
	std::printf("An empty quiver\n");
	ArcheryState st;
	ArcheryInput in = At(kShoulder, false, true);
	in.haveArrows = false;
	StepArchery(st, in, kSettings);
	in.drawGrip = true;
	ArcheryVerdict v = StepArchery(st, in, kSettings);
	Check(!v.took && v.state == ArrowState::None && !v.claimsGrip,
	      "no arrows: the grip at the quiver takes none (no sound, nothing to draw)");
	in.haveArrows = true;
	in.drawGrip = false;
	StepArchery(st, in, kSettings);
	in.drawGrip = true;
	v = StepArchery(st, in, kSettings);
	Check(v.took, "arrows again: taken");
}

void TestNockBlend() {
	std::printf("The hand eased onto the string\n");
	Check(Near(StepNockBlend(0.0f, true, 0.1f), 0.5f) && Near(StepNockBlend(0.9f, true, 0.1f), 1.0f),
	      "up by the frame's share of 0.2 s, no further than 1");
	Check(Near(StepNockBlend(0.7f, false, 0.1f), 0.0f), "off the string: at once back to 0");
	Check(Near(StepNockBlend(0.3f, true, -1.0f), 0.3f), "no time: no way");
	Check(Near(NockBlendWeight(0.0f), 0.0f) && Near(NockBlendWeight(1.0f), 1.0f) &&
	          Near(NockBlendWeight(0.5f), 0.5f) && NockBlendWeight(0.1f) < 0.1f && NockBlendWeight(2.0f) <= 1.0f,
	      "eased in and out, held to 0..1");

	const NiMatrix33 id = NiMatrix33::Identity();
	const NiPoint3 rest{0, 100, 0};
	const NiPoint3 axis{0, 1, 0};
	ArrowOnString on;
	ArrowOnBowLine(rest, axis, id, NiPoint3{0, 80, 0}, 46.6f, 15.6f, 28.0f, on);
	const NiPoint3 fistNock{10, 70, 5};
	const NiPoint3 grip{10, 73, 5};
	ArrowOnString e;
	Check(ArrowEasedOntoString(fistNock, on, rest, axis, id, grip, 46.6f, 0.0f, e) &&
	          NearPoint(e.pose.nock, fistNock) && NearPoint(e.gripTarget, grip) && Near(e.weight, 0.0f),
	      "at the start: the nock where the fist has it, the fist where it is, the string at rest");
	const NiPoint3 toRest{-10, 30, -5};
	const float l = std::sqrt(100.0f + 900.0f + 25.0f);
	Check(NearPoint(e.pose.rot * NiPoint3{0, 1, 0}, toRest * (1.0f / l)), "pointing through the arrow's rest");
	Check(ArrowEasedOntoString(fistNock, on, rest, axis, id, grip, 46.6f, 0.5f, e) &&
	          NearPoint(e.pose.nock, (fistNock + on.pose.nock) * 0.5f) &&
	          NearPoint(e.gripTarget, (grip + on.gripTarget) * 0.5f),
	      "halfway: halfway there");
	Check(ArrowEasedOntoString(fistNock, on, rest, axis, id, grip, 46.6f, 1.0f, e) &&
	          NearPoint(e.pose.nock, on.pose.nock) && NearPoint(e.gripTarget, on.gripTarget) && Near(e.weight, on.weight),
	      "at the end: the string's pose exactly");
	Check(ArrowEasedOntoString(rest, on, rest, axis, id, grip, 46.6f, 0.0f, e) &&
	          NearPoint(e.pose.rot * NiPoint3{0, 1, 0}, axis),
	      "the nock on the rest itself: along the bow");
	Check(!ArrowEasedOntoString(fistNock, on, rest, axis, id, grip, 0.0f, 0.5f, e) &&
	          !ArrowEasedOntoString(fistNock, on, rest, axis, id, grip, 0.0f, 1.0f, e),
	      "a model with no length: no pose");
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
	TestEase();
	TestWideNock();
	TestReleaseSnap();
	TestBowPower();
	TestNoArrows();
	TestNockBlend();
	TestBowSounds();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
