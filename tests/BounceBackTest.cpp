// Checks thrown gear coming back to the hand (game/BounceBackLogic.h): only
// weapons and torches, the lob that reaches the hand under gravity, its time
// from the rebound's speed held in bounds, the steering towards the hand from
// where the thing is now, capped, and every way it ends - near the hand, its
// time up, gone.

#include <cmath>
#include <cstdio>
#include <limits>

#include "game/BounceBackLogic.h"

using namespace obvr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr float kPerMetre = 70.0f;

float Length(const NiPoint3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

// Flies a point from `at` with `v` for `seconds` under gravity, in small steps.
NiPoint3 Fly(NiPoint3 at, NiPoint3 v, float seconds, float gravityUnits) {
	const int steps = 1000;
	const float dt = seconds / static_cast<float>(steps);
	for (int i = 0; i < steps; ++i) {
		at = at + v * dt;
		v.z -= gravityUnits * dt;
	}
	return at;
}

void TestWhatComesBack() {
	std::printf("What comes back\n");
	game::BounceBackSettings s;
	Check(game::BounceBackFor(s, game::BounceThing::Weapon), "a weapon comes back");
	Check(game::BounceBackFor(s, game::BounceThing::Torch), "a torch comes back");
	Check(!game::BounceBackFor(s, game::BounceThing::Other), "a cup or a potion does not");
	s.enabled = false;
	Check(!game::BounceBackFor(s, game::BounceThing::Weapon), "[Hands] BounceBack 0: nothing comes back");
}

void TestLob() {
	std::printf("The lob reaches the hand\n");
	const NiPoint3 from{300.0f, 0.0f, 100.0f};
	const NiPoint3 to{0.0f, 0.0f, 120.0f};
	const float g = 7.4f * kPerMetre;
	const NiPoint3 v = game::LobVelocity(from, to, 0.6f, g);
	const NiPoint3 end = Fly(from, v, 0.6f, g);
	Check(Length(end - to) < 2.0f, "flown 0.6 s under gravity it ends at the hand");
	Check(v.z > 0.0f, "it leaves upwards: an arc");
	Check(v.x < 0.0f, "towards the hand");
	const NiPoint3 tiny = game::LobVelocity(from, to, 0.0f, g);
	Check(std::isfinite(tiny.x) && std::isfinite(tiny.z), "no time given: still a finite velocity (a floor of 0.05 s)");
}

void TestFlightTime() {
	std::printf("The flight's time\n");
	const game::BounceBackSettings s;
	// 6 m/s hit, share 0.5: 3 m/s back over 2 m = 0.667 s.
	const float t = game::BounceFlightSeconds(s, 2.0f * kPerMetre, 6.0f, kPerMetre);
	Check(t > 0.66f && t < 0.67f, "2 m at half of a 6 m/s hit: 0.67 s");
	Check(game::BounceFlightSeconds(s, 10.0f * kPerMetre, 6.0f, kPerMetre) == s.maxFlightSeconds,
	      "far off: no longer than maxFlightSeconds");
	Check(game::BounceFlightSeconds(s, 0.2f * kPerMetre, 30.0f, kPerMetre) == s.minFlightSeconds,
	      "close by: no shorter than minFlightSeconds");
	// A fast hit is held at maxSpeed: 40 m/s * 0.5 = 20, held at 7 - 3.5 m takes 0.5 s.
	const float fast = game::BounceFlightSeconds(s, 3.5f * kPerMetre, 40.0f, kPerMetre);
	Check(fast > 0.49f && fast < 0.51f, "a fast hit comes back no faster than maxSpeed");
	// A slow one at minSpeed: 1 m/s * 0.5 held at 3 - 1.5 m takes 0.5 s.
	const float slow = game::BounceFlightSeconds(s, 1.5f * kPerMetre, 1.0f, kPerMetre);
	Check(slow > 0.49f && slow < 0.51f, "a slow hit comes back at least at minSpeed");
	const float nan = game::BounceFlightSeconds(s, 1.5f * kPerMetre, std::numeric_limits<float>::quiet_NaN(),
	                                            kPerMetre);
	Check(nan > 0.49f && nan < 0.51f, "a hit speed not a number: taken as none, minSpeed");
}

void TestBegin() {
	std::printf("At the hit\n");
	game::BounceBackSettings s;
	game::BounceFlight f;
	NiPoint3 v{0.0f, 0.0f, 0.0f};
	const NiPoint3 at{200.0f, 0.0f, 100.0f};
	const NiPoint3 hand{0.0f, 0.0f, 110.0f};
	Check(game::BeginBounce(f, s, 0x1234, at, hand, 6.0f, kPerMetre, v), "a hit begins the flight back");
	Check(f.ref == 0x1234 && f.flightSeconds > 0.0f, "the thing and its time kept");
	Check(f.startMetres > 2.8f && f.startMetres < 2.9f, "the hand's distance at the hit, metres");
	Check(v.x < 0.0f && v.z > 0.0f, "sent towards the hand, upwards");
	const NiPoint3 end = Fly(at, v, f.flightSeconds, s.gravityMetresPerSecond2 * kPerMetre);
	Check(Length(end - hand) < 2.0f, "left alone, the lob ends at the hand");
	s.enabled = false;
	Check(!game::BeginBounce(f, s, 0x1234, at, hand, 6.0f, kPerMetre, v) && f.ref == 0, "off: no flight");
	s.enabled = true;
	Check(!game::BeginBounce(f, s, 0, at, hand, 6.0f, kPerMetre, v), "no thing: no flight");
	Check(!game::BeginBounce(f, s, 0x1234, at, hand, 6.0f, 0.0f, v), "no scale: no flight");
}

void TestSteering() {
	std::printf("Steered on the way\n");
	const game::BounceBackSettings s;
	const float g = s.gravityMetresPerSecond2 * kPerMetre;
	const NiPoint3 hand{0.0f, 0.0f, 110.0f};
	{
		// Knocked off its lob (a foe's body in the way): turned back to it, a little a frame.
		game::BounceFlight f;
		NiPoint3 v{0.0f, 0.0f, 0.0f};
		NiPoint3 at{200.0f, 0.0f, 100.0f};
		game::BeginBounce(f, s, 7, at, hand, 6.0f, kPerMetre, v);
		NiPoint3 knocked{0.0f, 150.0f, 0.0f};  // sideways, nothing towards the hand
		const game::BounceStep held = game::StepBounce(f, s, true, at, knocked, hand, 1.0f / 72.0f, kPerMetre);
		const NiPoint3 lob = game::LobVelocity(at, hand, f.flightSeconds - f.seconds, g);
		Check(held.steer && Length(held.velocity - lob) < 0.5f,
		      "within holdSeconds of the hit: set to the lob outright, whatever the contact made of it");
		f.seconds = s.holdSeconds;  // past the hold
		const game::BounceStep st = game::StepBounce(f, s, true, at, knocked, hand, 1.0f / 72.0f, kPerMetre);
		Check(st.steer && st.end == game::BounceEnd::None, "steered while far from the hand");
		Check(st.velocity.x < knocked.x, "turned towards the hand");
		const float change = Length(st.velocity - knocked);
		Check(change <= s.guideMostMetresPerSecond2 * kPerMetre / 72.0f + 0.01f, "the change capped a frame");
		Check(change > 0.0f, "but some change");
	}
	{
		// On its lob already: the steering keeps it there.
		game::BounceFlight f;
		NiPoint3 v{0.0f, 0.0f, 0.0f};
		NiPoint3 at{200.0f, 0.0f, 100.0f};
		game::BeginBounce(f, s, 7, at, hand, 6.0f, kPerMetre, v);
		const float dt = 1.0f / 72.0f;
		float most = 0.0f;
		game::BounceEnd end = game::BounceEnd::None;
		for (int i = 0; i < 200 && end == game::BounceEnd::None; ++i) {
			const game::BounceStep st = game::StepBounce(f, s, true, at, v, hand, dt, kPerMetre);
			end = st.end;
			if (st.steer) {
				const float c = Length(st.velocity - v);
				most = c > most ? c : most;
				v = st.velocity;
			}
			at = at + v * dt;
			v.z -= g * dt;
		}
		Check(end == game::BounceEnd::AtHand, "flown and steered, it reaches the hand");
		Check(most < 0.2f * kPerMetre, "on its lob it is hardly steered");
	}
	{
		// Gravity off from the settings' guess: the steering still brings it.
		game::BounceFlight f;
		NiPoint3 v{0.0f, 0.0f, 0.0f};
		NiPoint3 at{250.0f, 40.0f, 90.0f};
		game::BeginBounce(f, s, 7, at, hand, 8.0f, kPerMetre, v);
		const float dt = 1.0f / 72.0f;
		const float realG = 9.8f * kPerMetre;
		game::BounceEnd end = game::BounceEnd::None;
		for (int i = 0; i < 200 && end == game::BounceEnd::None; ++i) {
			const game::BounceStep st = game::StepBounce(f, s, true, at, v, hand, dt, kPerMetre);
			end = st.end;
			if (st.steer) {
				v = st.velocity;
			}
			at = at + v * dt;
			v.z -= realG * dt;
		}
		Check(end == game::BounceEnd::AtHand, "gravity a third stronger than guessed: still reaches the hand");
	}
}

void TestEnds() {
	std::printf("How it ends\n");
	const game::BounceBackSettings s;
	const NiPoint3 hand{0.0f, 0.0f, 110.0f};
	NiPoint3 v{0.0f, 0.0f, 0.0f};
	{
		game::BounceFlight f;
		game::BeginBounce(f, s, 7, NiPoint3{200.0f, 0.0f, 100.0f}, hand, 6.0f, kPerMetre, v);
		const game::BounceStep st = game::StepBounce(f, s, false, NiPoint3{150.0f, 0.0f, 100.0f}, v, hand, 0.01f, kPerMetre);
		Check(st.end == game::BounceEnd::Gone && !st.steer && f.ref == 0, "the thing gone (caught, picked up): ended");
	}
	{
		game::BounceFlight f;
		game::BeginBounce(f, s, 7, NiPoint3{200.0f, 0.0f, 100.0f}, hand, 6.0f, kPerMetre, v);
		const NiPoint3 near{20.0f, 0.0f, 110.0f};  // 0.29 m
		const game::BounceStep st = game::StepBounce(f, s, true, near, v, hand, 0.01f, kPerMetre);
		Check(st.end == game::BounceEnd::AtHand && !st.steer && f.ref == 0, "near the hand: let go, flies on");
		Check(f.nearestMetres < 0.3f, "the nearest it came kept for the log");
	}
	{
		game::BounceFlight f;
		game::BeginBounce(f, s, 7, NiPoint3{200.0f, 0.0f, 100.0f}, hand, 6.0f, kPerMetre, v);
		const game::BounceStep st =
			game::StepBounce(f, s, true, NiPoint3{200.0f, 0.0f, 100.0f}, v, hand, s.guideSeconds + 0.1f, kPerMetre);
		Check(st.end == game::BounceEnd::TimeUp && !st.steer && f.ref == 0, "its time up: no longer steered");
	}
	{
		game::BounceFlight f;
		game::BeginBounce(f, s, 7, NiPoint3{200.0f, 0.0f, 100.0f}, hand, 6.0f, kPerMetre, v);
		const game::BounceStep st = game::StepBounce(f, s, true, NiPoint3{200.0f, 0.0f, 100.0f}, v, hand, 0.0f, kPerMetre);
		Check(st.end == game::BounceEnd::None && !st.steer && f.ref == 7, "a frame without time: nothing changed");
	}
	{
		game::BounceFlight f;
		const game::BounceStep st = game::StepBounce(f, s, true, hand, v, hand, 0.01f, kPerMetre);
		Check(st.end == game::BounceEnd::None && !st.steer, "no flight: nothing");
	}
	Check(game::BounceEndName(game::BounceEnd::AtHand)[0] != '\0' && game::BounceEndName(game::BounceEnd::TimeUp)[0] != '\0' &&
	          game::BounceEndName(game::BounceEnd::Gone)[0] != '\0' && game::BounceEndName(game::BounceEnd::None)[0] != '\0',
	      "every end has words");
}

}  // namespace

int main() {
	TestWhatComesBack();
	TestLob();
	TestFlightTime();
	TestBegin();
	TestSteering();
	TestEnds();
	if (g_failures != 0) {
		std::printf("%d failure(s)\n", g_failures);
		return 1;
	}
	std::printf("all ok\n");
	return 0;
}
