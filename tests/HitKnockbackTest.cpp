// The knockback the player's hits keep (game/HitKnockbackLogic.h): every
// flow of HitKnockbackShare.
#include <cstdio>
#include <limits>

#include "game/HitKnockbackLogic.h"

namespace {

using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

}  // namespace

int main() {
	std::printf("The knockback the player's hits keep\n");
	HitKnockbackSettings s;
	Check(Near(s.percent, 0.0f) && !s.powerOnly, "the defaults: none of the game's push, light and power alike");
	Check(Near(HitKnockbackShare(s, false, false, false), 1.0f), "another fighter's hit: the game's own, whatever the settings");
	Check(Near(HitKnockbackShare(s, false, true, true), 1.0f), "another fighter's power attack on the player: the game's own");
	Check(Near(HitKnockbackShare(s, true, true, false), 1.0f), "a hit on the player: left to NoPlayerStagger");
	Check(Near(HitKnockbackShare(s, true, false, false), 0.0f), "the player's light hit at 0 per cent: no push");
	Check(Near(HitKnockbackShare(s, true, false, true), 0.0f), "the player's power attack at 0 per cent: none either");
	s.percent = 100.0f;
	Check(Near(HitKnockbackShare(s, true, false, false), 1.0f), "100 per cent: the game's own push");
	s.percent = 50.0f;
	Check(Near(HitKnockbackShare(s, true, false, true), 0.5f), "50 per cent: half of it");
	s.percent = 150.0f;
	Check(Near(HitKnockbackShare(s, true, false, false), 1.0f), "over 100: held to the game's own");
	s.percent = -20.0f;
	Check(Near(HitKnockbackShare(s, true, false, false), 0.0f), "under 0: none");
	s.percent = std::numeric_limits<float>::quiet_NaN();
	Check(Near(HitKnockbackShare(s, true, false, false), 1.0f), "not a number: the game's own");
	s.percent = 100.0f;
	s.powerOnly = true;
	Check(Near(HitKnockbackShare(s, true, false, false), 0.0f), "power only: a light hit pushes nobody");
	Check(Near(HitKnockbackShare(s, true, false, true), 1.0f), "power only: a power attack pushes by the percent");
	s.percent = 40.0f;
	Check(Near(HitKnockbackShare(s, true, false, true), 0.4f), "power only at 40: a power attack pushes 0.4");
	Check(Near(HitKnockbackShare(s, false, false, false), 1.0f), "power only: another fighter's light hit still the game's own");
	Check(Near(HitKnockbackShare(s, true, true, false), 1.0f), "power only: a light hit on the player still NoPlayerStagger's");
	s.percent = 0.0f;
	Check(Near(HitKnockbackShare(s, true, false, true), 0.0f), "power only at 0: a power attack pushes nobody either");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
