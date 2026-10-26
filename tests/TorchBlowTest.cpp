// Checks the torch's blow (game/TorchBlowLogic.h): the other hand with the
// torch in it staggers whoever it meets fast enough, and knocks them down
// only with a power swing - at once when the swing already is one, held to
// the swing's end when it is not yet, as the weapon's strike by motion holds
// its bodies.

#include <cstdio>

#include "game/TorchBlowLogic.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestAtContact() {
	std::printf("The torch meets someone\n");
	TorchBlow b = TorchBlowAtContact(true, ShoveKind::None, true, true, false);
	Check(b.kind == ShoveKind::None && !b.hold, "too slow towards them: no blow, nothing held");
	b = TorchBlowAtContact(true, ShoveKind::Hard, false, false, false);
	Check(b.kind == ShoveKind::Light && !b.hold,
	      "past the hard shove's speed with no swing running: a stagger, not a knockdown");
	b = TorchBlowAtContact(true, ShoveKind::Light, false, false, false);
	Check(b.kind == ShoveKind::Light && !b.hold, "past the shove's speed with no swing running: a stagger");
	b = TorchBlowAtContact(true, ShoveKind::Hard, true, false, false);
	Check(b.kind == ShoveKind::None && b.hold, "a swing not yet a power one: held to its end");
	b = TorchBlowAtContact(true, ShoveKind::Light, true, true, false);
	Check(b.kind == ShoveKind::Hard && !b.hold, "a power swing already: knocked down at once, whatever the speed");
	b = TorchBlowAtContact(true, ShoveKind::Light, false, false, true);
	Check(b.kind == ShoveKind::Hard && !b.hold, "met on the frame a power swing ended: knocked down");
}

void TestSpeedAlone() {
	std::printf("TorchKnockdownPowerOnly off: the speed alone, as the off hand's strike\n");
	TorchBlow b = TorchBlowAtContact(false, ShoveKind::Hard, true, false, false);
	Check(b.kind == ShoveKind::Hard && !b.hold, "the hard shove's speed: knocked down at once, nothing held");
	b = TorchBlowAtContact(false, ShoveKind::Light, true, true, false);
	Check(b.kind == ShoveKind::Light && !b.hold, "the shove's speed in a power swing: still a stagger");
	b = TorchBlowAtContact(false, ShoveKind::None, true, true, true);
	Check(b.kind == ShoveKind::None && !b.hold, "too slow: no blow");
}

void TestSettle() {
	std::printf("A held blow settled\n");
	Check(SettleTorchBlow(7, 7, true, false, false) == ShoveKind::None,
	      "its swing still running, short of a power one: still held");
	Check(SettleTorchBlow(7, 7, true, true, false) == ShoveKind::Hard,
	      "its swing grew into a power one: knocked down now, not at its end");
	Check(SettleTorchBlow(7, 7, false, false, true) == ShoveKind::Hard, "its swing ended a power one: knocked down");
	Check(SettleTorchBlow(7, 7, false, false, false) == ShoveKind::Light,
	      "its swing ended light (or a twitch): a stagger");
	Check(SettleTorchBlow(7, 8, true, true, false) == ShoveKind::Light,
	      "its swing long gone, another one running: a stagger, not the new swing's power");
}

}  // namespace

int main() {
	TestAtContact();
	TestSpeedAlone();
	TestSettle();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
