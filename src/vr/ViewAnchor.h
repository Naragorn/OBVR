#pragma once

// The top and bottom rows of the view and how they follow the head
// (docs/hud-on-hands-spec.md, section 3): where a row was placed, when the
// head has turned far enough to take it along, and its glide after the head.
// Two ways: the level one (heading only; the big menu quad after the body,
// vr/MenuFacing.h) and the one that follows the view's pitch as well (the
// rows of text, [HandHud] ViewFollowsPitch).
//
// Pure: no game, no compositor. Covered by hand_hud_test.

#include "core/MathFns.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {

// The head's pitch in degrees, up positive: its forward (-z column) against
// the horizontal.
inline float HeadPitchDegrees(const openvr::HmdMatrix34& head) {
	float up = -head.m[1][2];
	up = up > 1.0f ? 1.0f : (up < -1.0f ? -1.0f : up);
	return math::Asin(up) * (180.0f / math::kPi);
}

// The head's heading and another's, apart by this many degrees (yaw only).
inline float HeadingApartDegrees(const openvr::HmdMatrix34& a, const openvr::HmdMatrix34& b) {
	const float ax = -a.m[0][2], az = -a.m[2][2];
	const float bx = -b.m[0][2], bz = -b.m[2][2];
	const float la = math::Sqrt(ax * ax + az * az);
	const float lb = math::Sqrt(bx * bx + bz * bz);
	if (la < 1e-4f || lb < 1e-4f) {
		return 0.0f;  // straight up or down: no heading to be apart by
	}
	const float dot = ax * bx + az * bz;
	float cross = ax * bz - az * bx;
	cross = cross < 0.0f ? -cross : cross;
	return math::Atan2(cross, dot) * (180.0f / math::kPi);
}

// A level pose looking along (fx, 0, fz), unit length, at (px, py, pz).
inline openvr::HmdMatrix34 LevelHeadingPose(float fx, float fz, float px, float py, float pz) {
	openvr::HmdMatrix34 m{};
	// z = -forward, y up, x = y cross z.
	m.m[0][0] = -fz;
	m.m[2][0] = fx;
	m.m[1][1] = 1.0f;
	m.m[0][2] = -fx;
	m.m[2][2] = -fz;
	m.m[0][3] = px;
	m.m[1][3] = py;
	m.m[2][3] = pz;
	return m;
}

// The angle between the two poses' view directions (their -z columns), in
// degrees, along the great circle: a turn and a nod count together, and
// straight up or down needs no heading.
inline float ViewDirectionApartDegrees(const openvr::HmdMatrix34& a, const openvr::HmdMatrix34& b) {
	const float ax = -a.m[0][2], ay = -a.m[1][2], az = -a.m[2][2];
	const float bx = -b.m[0][2], by = -b.m[1][2], bz = -b.m[2][2];
	const float cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
	const float cross = math::Sqrt(cx * cx + cy * cy + cz * cz);
	return math::Atan2(cross, ax * bx + ay * by + az * bz) * (180.0f / math::kPi);
}

// A pose looking along the heading (fx, fz), unit length and level, pitched
// `pitchDegrees` up (negative: down), at (px, py, pz); its x axis stays level
// - no roll.
inline openvr::HmdMatrix34 PitchedHeadingPose(float fx, float fz, float pitchDegrees, float px, float py,
                                              float pz) {
	const float p = pitchDegrees * (math::kPi / 180.0f);
	const float cp = math::Cos(p), sp = math::Sin(p);
	const float dx = fx * cp, dy = sp, dz = fz * cp;  // forward
	openvr::HmdMatrix34 m{};
	const float xx = -fz, xz = fx;  // level, to the right of the heading
	// y = z cross x, z = -forward, x = (xx, 0, xz).
	m.m[0][0] = xx;
	m.m[2][0] = xz;
	m.m[0][1] = (-dy) * xz;
	m.m[1][1] = (-dz) * xx - (-dx) * xz;
	m.m[2][1] = -((-dy) * xx);
	m.m[0][2] = -dx;
	m.m[1][2] = -dy;
	m.m[2][2] = -dz;
	m.m[0][3] = px;
	m.m[1][3] = py;
	m.m[2][3] = pz;
	return m;
}

// How high above the horizon a row hangs when its anchor looks `pitchDegrees`
// up: `rowDegrees` above (below, negative) where the anchor looks, no further
// than straight up or down.
inline float ViewRowElevationDegrees(float pitchDegrees, float rowDegrees) {
	const float e = pitchDegrees + rowDegrees;
	return e > 90.0f ? 90.0f : (e < -90.0f ? -90.0f : e);
}

// A glide ends this close to the head's heading.
inline constexpr float kViewSettledDegrees = 2.0f;

// A top or bottom row's anchor for this frame. Taken from the head when the
// row appears, or every frame when locked to the head; dropped when the row
// has nothing to show, so the next one appears where the head looks then.
// The head turned more than `followDegrees` from it (0: never) takes it
// along: gliding after the head with the time constant `followSeconds` -
// its heading and its place, level - until within kViewSettledDegrees,
// where it stands again; 0 seconds takes it there at once. Meta's design
// guide: "Anchor information ... to a space, or loosely follow the user
// using smoothing animation" (developers.meta.com/vr/design/
// mr-design-guideline); before, it jumped back to the middle.
struct HudViewAnchor {
	bool valid = false;
	bool following = false;
	openvr::HmdMatrix34 head{};
};
// The pitch-following glide (StepViewAnchor with followPitch): starts when
// the view direction is more than `followDegrees` away from the anchor's
// (heading and pitch together, great circle), then glides after the head with
// the time constant `followSeconds` in heading and in pitch, and its place,
// until within kViewSettledDegrees. The heading of a view straight up or down
// is none: the other's is taken, or the anchor's kept; the pitch glides
// either way, so nothing jumps and nothing divides by zero.
inline void GlideViewAnchorWithPitch(HudViewAnchor& a, const openvr::HmdMatrix34& head, float followDegrees,
                                     float followSeconds, float dt) {
	if (followDegrees > 0.0f && ViewDirectionApartDegrees(a.head, head) > followDegrees) {
		a.following = true;
	}
	if (!a.following) {
		return;
	}
	if (!(followSeconds > 0.0f)) {
		a.head = head;
		a.following = false;
		return;
	}
	if (!(dt > 0.0f)) {
		return;  // no time passed: held
	}
	const float share = dt / (followSeconds + dt);
	float ax = -a.head.m[0][2], az = -a.head.m[2][2];
	float hx = -head.m[0][2], hz = -head.m[2][2];
	const float la = math::Sqrt(ax * ax + az * az);
	const float lh = math::Sqrt(hx * hx + hz * hz);
	float fx = 0.0f, fz = -1.0f;  // no heading anywhere: any will do, the direction is the same
	float angle = 0.0f;
	if (la >= 1e-4f) {
		fx = ax / la, fz = az / la;
		if (lh >= 1e-4f) {
			const float gx = hx / lh, gz = hz / lh;
			angle = math::Atan2(fx * gz - fz * gx, fx * gx + fz * gz) * share;
		}
	} else if (lh >= 1e-4f) {
		fx = hx / lh, fz = hz / lh;
	}
	const float c = math::Cos(angle), s = math::Sin(angle);
	const float pitch = HeadPitchDegrees(a.head);
	const float towards = HeadPitchDegrees(head);
	a.head = PitchedHeadingPose(fx * c - fz * s, fx * s + fz * c, pitch + (towards - pitch) * share,
	                            a.head.m[0][3] + (head.m[0][3] - a.head.m[0][3]) * share,
	                            a.head.m[1][3] + (head.m[1][3] - a.head.m[1][3]) * share,
	                            a.head.m[2][3] + (head.m[2][3] - a.head.m[2][3]) * share);
	if (ViewDirectionApartDegrees(a.head, head) <= kViewSettledDegrees) {
		a.following = false;
	}
}

inline void StepViewAnchor(HudViewAnchor& a, bool shown, bool haveHead, const openvr::HmdMatrix34& head,
                           float followDegrees, bool lockedToHead, float followSeconds = 0.0f, float dt = 0.0f,
                           bool followPitch = false) {
	if (!shown || !haveHead) {
		a.valid = shown && a.valid;  // no head this frame: keep what there is
		a.following = a.following && a.valid;
		return;
	}
	if (!a.valid || lockedToHead) {
		a.head = head;
		a.valid = true;
		a.following = false;
		return;
	}
	if (followPitch) {
		GlideViewAnchorWithPitch(a, head, followDegrees, followSeconds, dt);
		return;
	}
	if (followDegrees > 0.0f && HeadingApartDegrees(a.head, head) > followDegrees) {
		a.following = true;
	}
	if (!a.following) {
		return;
	}
	if (!(followSeconds > 0.0f)) {
		a.head = head;
		a.following = false;
		return;
	}
	const float ax = -a.head.m[0][2], az = -a.head.m[2][2];
	const float hx = -head.m[0][2], hz = -head.m[2][2];
	const float la = math::Sqrt(ax * ax + az * az);
	const float lh = math::Sqrt(hx * hx + hz * hz);
	if (!(dt > 0.0f) || la < 1e-4f || lh < 1e-4f) {
		return;  // no time, or straight up or down: no heading to glide to
	}
	const float share = dt / (followSeconds + dt);
	// The signed angle from the anchor's heading to the head's, a share of it.
	const float fx = ax / la, fz = az / la;
	const float gx = hx / lh, gz = hz / lh;
	const float angle = math::Atan2(fx * gz - fz * gx, fx * gx + fz * gz) * share;
	const float c = math::Cos(angle), s = math::Sin(angle);
	a.head = LevelHeadingPose(fx * c - fz * s, fx * s + fz * c,
	                          a.head.m[0][3] + (head.m[0][3] - a.head.m[0][3]) * share,
	                          a.head.m[1][3] + (head.m[1][3] - a.head.m[1][3]) * share,
	                          a.head.m[2][3] + (head.m[2][3] - a.head.m[2][3]) * share);
	if (HeadingApartDegrees(a.head, head) <= kViewSettledDegrees) {
		a.following = false;
	}
}


}  // namespace obvr::vr
