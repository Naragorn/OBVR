#pragma once

namespace obvr {

// Easing towards a target, for the camera movements OBVR generates itself.
//
// Deliberately not used on the head. Smoothing a tracked head means showing
// the user where their head was rather than where it is, and in a headset that
// latency is felt directly - it is the one thing VR cannot trade away. What is
// worth easing is the artificial motion: a camera that the stick or the mouse
// drives, which has no counterpart in the inner ear and is where the sickness
// comes from.

// A fixed share of the remaining distance per second, that share being
// speedPerSecond * deltaSeconds.
//
// Per second rather than per frame, which is how UEVR computes its camera lerp
// as well - "t = m_lerp_camera_speed->value() * delta" in
// VR::on_pre_calculate_stereo_view_offset. A per-frame share would make the
// same setting feel twice as sluggish at 30 fps as at 60.
//
// It never quite arrives, which is exactly what makes it smooth. A missing
// frame time or a speed of zero means no smoothing rather than a frozen
// camera: of the ways to handle a caller with no timing, that is the only one
// that still follows the target.
float Approach(float current, float target, float speedPerSecond, float deltaSeconds);

// A heading, held as the cosine and sine of its angle rather than as the angle
// itself.
//
// Two reasons. Angles have a seam at 360 degrees, and easing across it the
// naive way sends the camera the long way round - UEVR needs a dedicated
// lerp_angle for precisely that. A pair of components has no seam: easing
// between two of them and renormalising always takes the short way, because
// the straight line between two points on a circle is on the short side of it.
//
// And the caller has cos and sin to hand anyway. They are read straight out of
// the camera matrix, so turning them into an angle and back would mean an
// atan2 and a sin/cos pair per frame for nothing.
struct Heading {
	float cosine = 1.0f;
	float sine = 0.0f;
};

// Eases one heading towards another and renormalises.
//
// Headings pointing opposite ways have no short way round to take, and near
// that point the easing is not merely ambiguous but wrong: the straight line
// between them passes through the origin, so a step shorter than halfway
// renormalises back onto the heading it started from and the camera does not
// move at all. It would sit still and then flip. Such headings are therefore
// recognised before the easing rather than after it, and the target is taken
// as it stands - a camera that turned right round in one frame was cut, not
// panned.
Heading Approach(const Heading& current, const Heading& target, float speedPerSecond,
                 float deltaSeconds);

}  // namespace obvr
