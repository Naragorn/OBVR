// Checks the top and bottom rows following the view's pitch as well as its
// heading (vr/ViewAnchor.h, [HandHud] ViewFollowsPitch; Nadi, 2026-11-28:
// the notices could not be read when looking down).

#include <cstdio>

#include "vr/HandHud.h"

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

bool Near(float a, float b, float eps = 0.05f) { return a - b < eps && b - a < eps; }

bool Finite(float v) { return v == v && v < 1e6f && v > -1e6f; }

constexpr float kDt = 1.0f / 90.0f;
constexpr float kDegrees = math::kPi / 180.0f;

// A head 1.7 m up, turned `yaw` degrees to the right and pitched `pitch`
// degrees up (negative: down), and rolled `roll` degrees about its forward.
openvr::HmdMatrix34 Head(float yaw, float pitch, float roll = 0.0f) {
	const float t = yaw * kDegrees, p = pitch * kDegrees;
	// Forward f = (sin t cos p, sin p, -cos t cos p); z = -f; x level.
	const float fx = math::Sin(t) * math::Cos(p), fy = math::Sin(p), fz = -math::Cos(t) * math::Cos(p);
	float xx = math::Cos(t), xy = 0.0f, xz = math::Sin(t);
	// y = z cross x.
	float yx = (-fy) * xz - (-fz) * xy;
	float yy = (-fz) * xx - (-fx) * xz;
	float yz = (-fx) * xy - (-fy) * xx;
	if (roll != 0.0f) {  // x and y turned about z
		const float c = math::Cos(roll * kDegrees), s = math::Sin(roll * kDegrees);
		const float nxx = xx * c + yx * s, nxy = xy * c + yy * s, nxz = xz * c + yz * s;
		const float nyx = yx * c - xx * s, nyy = yy * c - xy * s, nyz = yz * c - xz * s;
		xx = nxx, xy = nxy, xz = nxz, yx = nyx, yy = nyy, yz = nyz;
	}
	openvr::HmdMatrix34 h{};
	h.m[0][0] = xx, h.m[1][0] = xy, h.m[2][0] = xz;
	h.m[0][1] = yx, h.m[1][1] = yy, h.m[2][1] = yz;
	h.m[0][2] = -fx, h.m[1][2] = -fy, h.m[2][2] = -fz;
	h.m[1][3] = 1.7f;
	return h;
}

float AnchorPitch(const HudViewAnchor& a) { return HeadPitchDegrees(a.head); }

float AnchorYaw(const HudViewAnchor& a) {
	return math::Atan2(-a.head.m[0][2], a.head.m[2][2]) / kDegrees;
}

// One frame of the pitch-following step, 20 degrees, 0.4 s.
void Step(HudViewAnchor& a, const openvr::HmdMatrix34& head, bool locked = false, float seconds = 0.4f,
          bool pitch = true, bool shown = true, bool haveHead = true) {
	StepViewAnchor(a, shown, haveHead, head, 20.0f, locked, seconds, kDt, pitch);
}

// Frames until the glide ends, up to 900.
int Settle(HudViewAnchor& a, const openvr::HmdMatrix34& head) {
	int frames = 0;
	while (a.following && frames < 900) {
		Step(a, head);
		++frames;
	}
	return frames;
}

void TestAppearAndThreshold() {
	std::printf("A row appears where the view points, pitch included\n");
	HudViewAnchor a;
	Step(a, Head(0.0f, -60.0f));
	Check(a.valid && Near(AnchorPitch(a), -60.0f), "it appears at the view's pitch (looking down 60)");
	Check(Near(ViewDirectionApartDegrees(a.head, Head(0.0f, -60.0f)), 0.0f), "no angle apart from where it stands");

	HudViewAnchor b;
	Step(b, Head(0.0f, 0.0f));
	Step(b, Head(0.0f, 15.0f));
	Check(!b.following && Near(AnchorPitch(b), 0.0f), "pitch only, below 20: it stays");
	Step(b, Head(0.0f, 30.0f));
	Check(b.following, "pitch only, above 20: it starts to glide");
	Check(AnchorPitch(b) > 0.1f && AnchorPitch(b) < 5.0f, "a little a frame, no jump");
	Check(Near(AnchorYaw(b), 0.0f), "heading unchanged by a pitch glide");
	const int frames = Settle(b, Head(0.0f, 30.0f));
	Check(!b.following && Near(AnchorPitch(b), 30.0f, kViewSettledDegrees + 0.01f), "it settles at the new pitch");
	Check(frames > 45 && frames < 270, "over about a second, not at once");
	Check(Near(ViewDirectionApartDegrees(b.head, Head(0.0f, 30.0f)), 0.0f, kViewSettledDegrees + 0.01f),
	      "settled within kViewSettledDegrees of the view");

	HudViewAnchor c;
	Step(c, Head(0.0f, 0.0f));
	Step(c, Head(12.0f, 12.0f));
	Check(!c.following, "12 degrees across and 12 up (17 apart): below 20, it stays");
	Check(Near(ViewDirectionApartDegrees(c.head, Head(12.0f, 12.0f)), 16.9f, 0.2f), "that is 16.9 degrees apart");
}

void TestHeadingAndBoth() {
	std::printf("Heading, both, and straight up or down\n");
	HudViewAnchor a;
	Step(a, Head(0.0f, 0.0f));
	Step(a, Head(40.0f, 0.0f));
	Check(a.following && AnchorYaw(a) > 0.5f && AnchorYaw(a) < 5.0f && Near(AnchorPitch(a), 0.0f),
	      "heading only above 20: glides in heading, level as before");
	Settle(a, Head(40.0f, 0.0f));
	Check(Near(AnchorYaw(a), 40.0f, kViewSettledDegrees + 0.01f), "and settles at the new heading");

	HudViewAnchor b;
	Step(b, Head(0.0f, 0.0f));
	Step(b, Head(30.0f, -30.0f));
	Check(b.following && AnchorYaw(b) > 0.1f && AnchorPitch(b) < -0.1f, "both at once: heading and pitch both glide");
	Settle(b, Head(30.0f, -30.0f));
	Check(!b.following && Near(AnchorYaw(b), 30.0f, 2.5f) && Near(AnchorPitch(b), -30.0f, 2.5f),
	      "both settle");

	HudViewAnchor c;
	Step(c, Head(0.0f, 0.0f));
	Step(c, Head(0.0f, -90.0f));
	Check(c.following && Finite(AnchorYaw(c)) && Finite(AnchorPitch(c)), "straight down: it glides, nothing broken");
	Settle(c, Head(0.0f, -90.0f));
	Check(!c.following && Near(AnchorPitch(c), -90.0f, 2.5f) && Near(AnchorYaw(c), 0.0f, 0.5f),
	      "it settles down there, heading kept");

	HudViewAnchor d;
	Step(d, Head(0.0f, 90.0f));
	Check(d.valid && Finite(d.head.m[0][0]) && Near(AnchorPitch(d), 90.0f, 1.0f), "it appears with the view straight up (math::Asin stops at 89.19)");
	Step(d, Head(70.0f, 0.0f));
	Check(d.following && Finite(AnchorYaw(d)) && Finite(AnchorPitch(d)), "from straight up to level: adopts the heading, glides");
	Settle(d, Head(70.0f, 0.0f));
	Check(Near(AnchorPitch(d), 0.0f, 2.5f) && Near(AnchorYaw(d), 70.0f, 2.5f), "and arrives");

	HudViewAnchor e;
	Step(e, Head(0.0f, 90.0f));
	Step(e, Head(0.0f, -90.0f));
	Check(e.following && Finite(AnchorYaw(e)) && Finite(AnchorPitch(e)), "from straight up to straight down: no heading anywhere, no NaN");
	Settle(e, Head(0.0f, -90.0f));
	Check(Near(AnchorPitch(e), -90.0f, 2.5f), "it arrives");

	HudViewAnchor f;
	Step(f, Head(0.0f, 0.0f));
	Step(f, Head(0.0f, 0.0f, 40.0f));
	Step(f, Head(0.0f, 30.0f, 40.0f));
	Check(f.following, "a rolled head: only its view direction counts");
	Settle(f, Head(0.0f, 30.0f, 40.0f));
	Check(Near(f.head.m[1][0], 0.0f) && Near(AnchorPitch(f), 30.0f, 2.5f), "the anchor carries no roll");
}

void TestSwitchesAndEdges() {
	std::printf("Switches and edges\n");
	HudViewAnchor a;
	Step(a, Head(0.0f, 0.0f), false, 0.4f, true);
	Step(a, Head(0.0f, 40.0f), false, 0.0f, true);
	Check(Near(AnchorPitch(a), 40.0f) && !a.following, "0 seconds: taken to the view's pitch at once");

	HudViewAnchor b;
	Step(b, Head(0.0f, 0.0f), false, 0.4f, false);
	Step(b, Head(0.0f, 40.0f), false, 0.4f, false);
	Check(!b.following, "the old behaviour: a look down alone takes it nowhere");
	Step(b, Head(40.0f, 40.0f), false, 0.4f, false);
	Check(b.following, "the old behaviour: heading alone still does");

	HudViewAnchor c;
	Step(c, Head(0.0f, 0.0f));
	Step(c, Head(0.0f, 50.0f), true);
	Check(Near(AnchorPitch(c), 50.0f) && !c.following, "locked to the head: its pitch every frame");

	HudViewAnchor d;
	Step(d, Head(0.0f, 0.0f));
	Step(d, Head(0.0f, 50.0f), false, 0.4f, true, true, false);
	Check(d.valid && Near(AnchorPitch(d), 0.0f) && !d.following, "no head this frame: kept");
	Step(d, Head(0.0f, 50.0f));
	Check(d.following, "the head back: it glides");
	Step(d, Head(0.0f, 50.0f), false, 0.4f, true, false);
	Check(!d.valid && !d.following, "nothing to show: dropped, the next one appears where the view points then");

	HudViewAnchor e;
	Step(e, Head(0.0f, 0.0f));
	StepViewAnchor(e, true, true, Head(0.0f, 50.0f), 20.0f, false, 0.4f, 0.0f, true);
	Check(e.following && Near(AnchorPitch(e), 0.0f), "no time passed: held, glide pending");

	HudViewAnchor g;
	Step(g, Head(0.0f, 0.0f));
	StepViewAnchor(g, true, true, Head(0.0f, 50.0f), 0.0f, false, 0.4f, kDt, true);
	Check(!g.following, "ViewFollowDegrees 0: never");

	HudViewAnchor h;
	Step(h, Head(0.0f, 0.0f));
	openvr::HmdMatrix34 moved = Head(0.0f, 0.0f);
	moved.m[0][3] = 1.0f;
	Step(h, Head(0.0f, 50.0f));
	moved = Head(0.0f, 50.0f);
	moved.m[0][3] = 1.0f;
	Step(h, moved);
	Check(h.head.m[0][3] > 0.0f && h.head.m[0][3] < 0.2f, "its place glides along with the head as before");
}

void TestPlacement() {
	std::printf("The rows' place\n");
	HandHudSettings s;
	Check(s.viewFollowsPitch, "the default: the rows follow the pitch");
	HandHudFrame f;
	f.haveHead = true;
	f.head = Head(0.0f, 0.0f);
	f.rect[7] = UiRect{48, 40, 602, 85, true};
	f.rect[8] = UiRect{653, 700, 1053, 745, true};
	f.viewAnchorValid[0] = f.viewAnchorValid[1] = true;
	f.viewAnchor[0] = f.viewAnchor[1] = Head(0.0f, -50.0f);
	HandHudQuad q[kHudElementCount];
	PlaceHandHud(s, f, q);
	const float topY = 1.7f + 1.2f * math::Sin((-50.0f + 15.0f) * kDegrees);
	const float botY = 1.7f + 1.2f * math::Sin((-50.0f - 20.0f) * kDegrees);
	Check(q[7].shown && Near(q[7].pose.m[1][3], topY, 0.005f), "looking down 50: the top row 15 above where the view points");
	Check(q[8].shown && Near(q[8].pose.m[1][3], botY, 0.005f), "and the bottom row 20 below");
	Check(Near(q[7].pose.m[1][0], 0.0f) && Near(q[8].pose.m[1][0], 0.0f), "no roll: the rows' own x stays level");
	s.viewFollowsPitch = false;
	PlaceHandHud(s, f, q);
	Check(Near(q[7].pose.m[1][3], 1.7f + 1.2f * math::Sin(15.0f * kDegrees), 0.005f), "the old behaviour: level, at the horizon's 15");
	s.viewFollowsPitch = true;
	f.viewAnchor[0] = Head(0.0f, 80.0f);
	PlaceHandHud(s, f, q);
	Check(Near(q[7].pose.m[1][3], 1.7f + 1.2f, 0.005f) && Finite(q[7].pose.m[0][3]) && Finite(q[7].pose.m[2][3]),
	      "looking nearly straight up: the top row stops at the zenith");
	f.viewAnchor[1] = Head(0.0f, -90.0f);
	PlaceHandHud(s, f, q);
	Check(Near(q[8].pose.m[1][3], 1.7f - 1.2f, 0.005f) && Finite(q[8].pose.m[0][3]),
	      "looking straight down: the bottom row stops at the nadir");
}

// The row must stay in the view at the existing threshold. Read from the
// logged raw projection of Nadi's Pimax Dream Air (OBVR.log, 2026-11-28
// 20:35): t -1.0913, b 1.0913 per eye, a vertical field of 95.0 degrees.
void TestThresholdFitsTheView() {
	std::printf("The threshold keeps the rows in the view\n");
	HandHudSettings s;
	const float halfFov = math::Atan2(1.0913f, 1.0f) / kDegrees;
	Check(Near(halfFov, 47.5f, 0.05f), "the headset's half vertical field: 47.5 degrees");
	// A row's worst offset from the view's middle is its own elevation plus
	// the pitch the view may be away before the glide starts.
	const float top = s.viewTopDegrees + s.viewFollowDegrees;
	const float bottom = s.viewBottomDegrees + s.viewFollowDegrees;
	std::printf("  worst offset: top %.1f, bottom %.1f of %.1f\n", static_cast<double>(top),
	            static_cast<double>(bottom), static_cast<double>(halfFov));
	Check(top < halfFov && bottom < halfFov, "at 20 degrees both rows' middles are inside the field");
	// A row 45 units tall at 0.0008 m per unit, 1.2 m away: its half height.
	const float halfHeight = math::Atan2(45.0f * s.viewUnitMetres * 0.5f, s.viewDistanceMetres) / kDegrees;
	Check(bottom + halfHeight < halfFov, "and a one-line row's edge too");
}

}  // namespace

int main() {
	TestAppearAndThreshold();
	TestHeadingAndBoth();
	TestSwitchesAndEdges();
	TestPlacement();
	TestThresholdFitsTheView();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
