#pragma once

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::vr::menu {

// Runtime-independent surface geometry. All positions share one tracking space.
inline bool Finite(float value) { return value >= -3.402823466e38f && value <= 3.402823466e38f; }
inline bool Finite(const NiPoint3& p) { return Finite(p.x) && Finite(p.y) && Finite(p.z); }
inline float Dot(const NiPoint3& a, const NiPoint3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline NiPoint3 Cross(const NiPoint3& a, const NiPoint3& b) {
	return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline bool Unit(const NiPoint3& v) {
	const float lengthSq = v.LengthSquared();
	return Finite(v) && lengthSq >= 0.999f && lengthSq <= 1.001f;
}

struct Surface {
	NiPoint3 centre{0, 0, 0};
	NiPoint3 right{1, 0, 0};
	NiPoint3 up{0, 1, 0};
	float width = 0;
	float height = 0;
	float pixelWidth = 0;
	float pixelHeight = 0;
};

inline bool Valid(const Surface& s) {
	const float orthogonal = Dot(s.right, s.up);
	return Finite(s.centre) && Unit(s.right) && Unit(s.up) &&
	       orthogonal >= -0.001f && orthogonal <= 0.001f &&
	       Finite(s.width) && s.width > 0 && Finite(s.height) && s.height > 0 &&
	       Finite(s.pixelWidth) && s.pixelWidth >= 1 &&
	       Finite(s.pixelHeight) && s.pixelHeight >= 1;
}

struct Contact {
	bool inside = false;
	float u = 0;
	float v = 0;
	float pixelX = 0;
	float pixelY = 0;
	float depth = 0; // Positive on the front of the surface.
};

inline Contact Project(const Surface& s, const NiPoint3& point) {
	Contact c;
	if (!Valid(s) || !Finite(point)) return c;
	const NiPoint3 local = point - s.centre;
	c.depth = Dot(local, Cross(s.right, s.up));
	c.u = Dot(local, s.right) / s.width + 0.5f;
	c.v = 0.5f - Dot(local, s.up) / s.height;
	if (!Finite(c.depth) || !(c.u >= 0 && c.u <= 1 && c.v >= 0 && c.v <= 1)) return {};
	c.inside = true;
	c.pixelX = c.u * (s.pixelWidth - 1);
	c.pixelY = c.v * (s.pixelHeight - 1);
	return c;
}

struct RayHit {
	Contact contact;
	float distance = 0;
};

inline RayHit Intersect(const Surface& s, const NiPoint3& origin, const NiPoint3& direction) {
	if (!Valid(s) || !Finite(origin) || !Unit(direction)) return {};
	const NiPoint3 normal = Cross(s.right, s.up);
	const float along = Dot(direction, normal);
	if (along >= -0.0001f) return {}; // Back-facing or parallel.
	const float distance = Dot(s.centre - origin, normal) / along;
	if (!Finite(distance) || distance <= 0) return {};
	const Contact contact = Project(s, origin + direction * distance);
	return contact.inside ? RayHit{contact, distance} : RayHit{};
}

struct TouchLimits {
	float press = 0.008f;
	float release = 0.018f;
	float hover = 0.05f;
	float penetration = 0.025f;
};
inline bool Valid(const TouchLimits& t) {
	return Finite(t.press) && Finite(t.release) && Finite(t.hover) && Finite(t.penetration) &&
	       t.press >= 0 && t.release > t.press && t.hover >= t.release && t.penetration >= 0;
}
enum class TouchPhase { Unarmed, Hover, Pressed };
struct TouchState { TouchPhase phase = TouchPhase::Unarmed; };
struct TouchResult { bool hover = false; bool held = false; bool down = false; bool up = false; };

// A fresh front-side approach arms touch. Appearing already embedded in a menu
// cannot select. Losing eligibility releases immediately and requires rearming.
inline TouchResult StepTouch(TouchState& state, const Contact& contact,
                             bool eligible, const TouchLimits& t = {}) {
	const bool wasPressed = state.phase == TouchPhase::Pressed;
	if (!eligible || !Valid(t) || !contact.inside || !Finite(contact.depth) ||
	    contact.depth < -t.penetration) {
		state.phase = TouchPhase::Unarmed;
		return {false, false, false, wasPressed};
	}
	if (contact.depth >= t.release) {
		state.phase = TouchPhase::Hover;
		return {contact.depth <= t.hover, false, false, wasPressed};
	}
	if (wasPressed) return {true, true, false, false};
	if (state.phase == TouchPhase::Hover && contact.depth <= t.press) {
		state.phase = TouchPhase::Pressed;
		return {true, true, true, false};
	}
	return {contact.depth >= 0 && contact.depth <= t.hover, false, false, false};
}

} // namespace obvr::vr::menu
