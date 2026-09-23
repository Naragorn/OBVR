#include <cstdio>
#include <cmath>
#include <limits>
#include <initializer_list>

#include "vr/ScrollGesture.h"

namespace {
using namespace obvr::vr::menu;
using obvr::NiPoint3;
int checks = 0;
int failures = 0;
const float nan = std::numeric_limits<float>::quiet_NaN();
const float inf = std::numeric_limits<float>::infinity();
void Check(bool value, const char* label) {
	++checks;
	if (!value) { ++failures; std::printf("FAIL: %s\n", label); }
}
bool Near(float a, float b) { return std::fabs(a-b) < 0.00001f; }

GestureFrame Frame() {
	GestureFrame f;
	f.left.tracked = f.right.tracked = f.head.tracked = true;
	f.left.position = {-0.06f, -0.2f, -0.35f};
	f.right.position = {0.06f, -0.2f, -0.35f};
	f.eligible = f.controlsNeutral = true;
	f.dt = 0.05f;
	return f;
}
Surface Panel() { return {{0, 0, -0.5f}, {1, 0, 0}, {0, 1, 0}, 0.6f, 0.4f, 1200, 800}; }

void TestMapping() {
	ScrollLimits t;
	Check(Valid(t), "default scroll limits valid");
	Check(MapOpening(0, t).amount == 0, "together is zero");
	Check(MapOpening(t.closedDistance, t).amount == 0, "closed boundary zero");
	for (int percent = 1; percent < 100; ++percent) {
		const float distance = t.closedDistance + (t.fullDistance-t.closedDistance)*percent/100.0f;
		const Opening a = MapOpening(distance, t);
		Check(a.valid && Near(a.amount, percent/100.0f) && !a.fullyOpen, "partial distance never interactive");
		for (int i = 0; i < 3; ++i)
			Check(MapOpening(distance, t).amount == a.amount, "stopped hands never advance opening");
	}
	Check(!MapOpening(std::nextafter(t.fullDistance, 0.0f), t).fullyOpen, "one float below full refuses activation");
	Check(MapOpening(t.fullDistance, t).fullyOpen && MapOpening(t.fullDistance, t).amount == 1,
	      "actual full distance activates");
	Check(MapOpening(1, t).amount == 1 && MapOpening(1, t).fullyOpen, "overshoot clamps at one");
	Check(MapOpening(0.34f, t).amount < MapOpening(0.5f, t).amount, "closing reverses immediately");
	for (float distance : {-1.0f, nan, inf, -inf}) Check(!MapOpening(distance, t).valid, "invalid distance refuses");
	float ScrollLimits::* fields[] = {&ScrollLimits::closedDistance, &ScrollLimits::poseDistance,
		&ScrollLimits::fullDistance, &ScrollLimits::dwellSeconds, &ScrollLimits::axisCosine,
		&ScrollLimits::forwardCosine, &ScrollLimits::minimumForward, &ScrollLimits::maximumForward,
		&ScrollLimits::minimumHeight, &ScrollLimits::maximumHeight};
	for (auto member : fields) {
		ScrollLimits bad; bad.*member = nan;
		Check(!Valid(bad) && !MapOpening(0.6f, bad).valid, "each nonfinite threshold refuses");
	}
	for (int i = 0; i < 9; ++i) {
		ScrollLimits bad;
		switch (i) {
		case 0: bad.closedDistance = 0; break;
		case 1: bad.poseDistance = bad.closedDistance; break;
		case 2: bad.fullDistance = bad.poseDistance; break;
		case 3: bad.dwellSeconds = 0; break;
		case 4: bad.axisCosine = 0; break;
		case 5: bad.forwardCosine = 1.01f; break;
		case 6: bad.minimumForward = 0; break;
		case 7: bad.maximumForward = bad.minimumForward; break;
		case 8: bad.maximumHeight = bad.minimumHeight; break;
		}
		Check(!Valid(bad), "invalid ordered threshold refuses");
	}
}

void TestGesture() {
	const auto horizontal = ScrollOrientation::Horizontal;
	const auto vertical = ScrollOrientation::Vertical;
	Check(ScrollPose(Frame(), horizontal), "horizontal pose accepted");
	Check(!ScrollPose(Frame(), vertical), "wrong axis refuses");
	Check(!ScrollPose(Frame(), ScrollOrientation::Off), "off never detects");
	Check(!ScrollPose(Frame(), static_cast<ScrollOrientation>(99)), "unknown mode refuses");
	for (int order : {-1, 1}) {
		auto f = Frame(); f.left.position = {0, -0.2f - order*0.06f, -0.35f};
		f.right.position = {0, -0.2f + order*0.06f, -0.35f};
		Check(ScrollPose(f, vertical), "either hand may be above in vertical mode");
	}
	for (int fault = 0; fault < 19; ++fault) {
		auto f = Frame();
		switch (fault) {
		case 0: f.eligible = false; break;
		case 1: f.left.tracked = false; break;
		case 2: f.right.tracked = false; break;
		case 3: f.head.tracked = false; break;
		case 4: f.left.position.x = nan; break;
		case 5: f.right.position = f.left.position; break;
		case 6: f.right.position.x = 0.3f; break;
		case 7: f.right.position.x = -0.18f; break;
		case 8: f.left.forward = {0, 0, 1}; break;
		case 9: f.left.up = {0, -1, 0}; break;
		case 10: f.left.forward = {0, 0, -2}; break;
		case 11: f.head.up = f.head.forward; break;
		case 12: f.left.position.z = f.right.position.z = -0.05f; break;
		case 13: f.left.position.z = f.right.position.z = -0.9f; break;
		case 14: f.left.position.y = f.right.position.y = -0.8f; break;
		case 15: f.left.position.y = f.right.position.y = 0.2f; break;
		case 16: f.left.forward = f.right.forward = {0, 0, 1}; break;
		case 17: f.right.position.x = inf; break;
		case 18: f.left.position.x = -3e38f; f.right.position.x = 3e38f; break;
		}
		Check(!ScrollPose(f, horizontal), "deliberate-pose refusal flow");
	}
	ReadyState state;
	auto f = Frame();
	for (int i = 0; i < 6; ++i) Check(!StepReady(state, f, horizontal).ready, "dwell prevents passing activation");
	auto r = StepReady(state, f, horizontal);
	Check(r.ready && r.entered, "dwell produces one readiness cue");
	Check(StepReady(state, f, horizontal).ready && !StepReady(state, f, horizontal).entered, "held readiness never repeats cue");
	f.controlsNeutral = false;
	Check(!StepReady(state, f, horizontal).ready && state.dwell == 0, "held grip or combat input cancels dwell");
	f = Frame(); f.left.tracked = false;
	Check(!StepReady(state, f, horizontal).ready, "tracking loss resets readiness");
	for (float dt : {0.0f, -1.0f, 0.101f, nan, inf}) {
		f = Frame(); f.dt = dt; state.dwell = 0.34f;
		Check(!StepReady(state, f, horizontal).ready && state.dwell == 0, "bad timing cannot complete dwell");
	}
}

void TestSurface() {
	auto s = Panel();
	Check(Valid(s), "valid menu surface");
	auto c = Project(s, s.centre);
	Check(c.inside && Near(c.u, 0.5f) && Near(c.v, 0.5f) && Near(c.pixelX, 599.5f), "centre maps to UI centre");
	c = Project(s, s.centre + s.right*0.3f + s.up*0.2f);
	Check(c.inside && Near(c.pixelX, 1199) && c.pixelY == 0, "top-right maps to last valid pixel");
	c = Project(s, s.centre - s.right*0.3f - s.up*0.2f);
	Check(c.inside && c.pixelX == 0 && Near(c.pixelY, 799), "bottom-left maps correctly");
	Check(!Project(s, {0.31f, 0, -0.5f}).inside && !Project(s, {0, 0.21f, -0.5f}).inside, "outside surface refuses");
	Check(!Project(s, {nan, 0, 0}).inside, "nonfinite point refuses");
	auto hit = Intersect(s, {0,0,0}, {0,0,-1});
	Check(hit.contact.inside && Near(hit.distance, 0.5f), "front ray hits in metres");
	Check(!Intersect(s, {0,0,-1}, {0,0,-1}).contact.inside, "intersection behind origin refuses");
	Check(!Intersect(s, {0,0,-1}, {0,0,1}).contact.inside, "back-side ray refuses");
	Check(!Intersect(s, {0,0,0}, {1,0,0}).contact.inside, "parallel ray refuses");
	Check(!Intersect(s, {0,0,0}, {0,0,-2}).contact.inside, "nonunit ray refuses");
	Check(!Intersect(s, {inf,0,0}, {0,0,-1}).contact.inside, "invalid origin refuses");
	Check(!Intersect(s, {1,0,0}, {0,0,-1}).contact.inside, "ray outside rectangle refuses");
	Check(!Intersect(s, s.centre, {0,0,-1}).contact.inside, "origin on plane refuses");
	s.centre = {1,2,3}; s.right = {0,0,-1}; s.up = {0,1,0};
	hit = Intersect(s, {1.5f,2,3}, {-1,0,0});
	Check(hit.contact.inside && Near(hit.contact.u, 0.5f), "rotated translated surface uses local coordinates");
	for (int fault = 0; fault < 9; ++fault) {
		s = Panel();
		switch (fault) {
		case 0: s.width = 0; break;
		case 1: s.height = -1; break;
		case 2: s.pixelWidth = 0; break;
		case 3: s.pixelHeight = nan; break;
		case 4: s.right = s.up; break;
		case 5: s.up.y = 2; break;
		case 6: s.centre.z = inf; break;
		case 7: s.width = inf; break;
		case 8: s.right.x = nan; break;
		}
		Check(!Valid(s) && !Project(s, {}).inside && !Intersect(s, {}, {0,0,-1}).contact.inside,
		      "invalid surface fails all mappings closed");
	}
}

void TestTouch() {
	TouchLimits t;
	TouchState state;
	Contact c; c.inside = true; c.depth = 0;
	Check(!StepTouch(state, c, true).held, "appearing on surface cannot click");
	c.depth = -0.001f;
	Check(!StepTouch(state, c, true).held, "approaching from behind cannot click");
	c.depth = 0.06f;
	Check(!StepTouch(state, c, true).hover, "outside hover radius");
	c.depth = t.hover;
	Check(StepTouch(state, c, true).hover, "hover boundary");
	c.depth = (t.press+t.release)/2;
	Check(!StepTouch(state, c, true).held, "hysteresis band cannot start press");
	c.depth = t.press;
	auto r = StepTouch(state, c, true);
	Check(r.down && r.held && !r.up, "press boundary begins touch");
	r = StepTouch(state, c, true);
	Check(!r.down && r.held, "resting touch never repeats down");
	c.depth = std::nextafter(t.release, 0.0f);
	Check(StepTouch(state, c, true).held, "held until release boundary");
	c.depth = t.release;
	r = StepTouch(state, c, true);
	Check(r.up && !r.held, "release at release boundary");
	for (int fault = 0; fault < 6; ++fault) {
		state.phase = TouchPhase::Pressed; c.inside = true; c.depth = 0;
		TouchLimits limits;
		switch (fault) {
		case 0: c.inside = false; break;
		case 1: c.depth = -0.026f; break;
		case 2: c.depth = nan; break;
		case 3: limits.release = limits.press; break;
		case 4: limits.hover = limits.press; break;
		case 5: limits.penetration = -1; break;
		}
		r = StepTouch(state, c, true, limits);
		Check(r.up && !r.held && state.phase == TouchPhase::Unarmed, "touch failure releases and disarms");
	}
	state.phase = TouchPhase::Pressed; c = {}; c.inside = true;
	r = StepTouch(state, c, false);
	Check(r.up && !r.held, "tracking or ownership loss releases drag");
	Check(!StepTouch(state, c, true).held, "restoring eligibility cannot reuse held contact");
	Check(!Valid(TouchLimits{-1,0.018f,0.05f,0.025f}), "negative touch depth refuses");
	Check(!Valid(TouchLimits{0.008f,inf,0.05f,0.025f}), "nonfinite touch threshold refuses");
}
} // namespace

int main() {
	TestMapping(); TestGesture(); TestSurface(); TestTouch();
	std::printf("Scroll menu: %d/%d checks passed\n", checks-failures, checks);
	return failures ? 1 : 0;
}
