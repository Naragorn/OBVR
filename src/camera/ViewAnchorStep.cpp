#include "camera/ViewAnchorStep.h"

#include "core/Log.h"

namespace obvr::camera {

namespace {

// Glide starts logged per session: enough to read the first look down and
// the first turn, never a line a frame.
constexpr UInt32 kGlideLogLines = 8;

const char* Cause(float headingApart, float pitchApart) {
	return pitchApart > headingApart ? "a nod" : "a turn";
}

}  // namespace

void StepViewRow(vr::HudViewAnchor& anchor, UInt32 row, bool shown, const vr::HandHudSettings& settings,
                 vr::HandHudFrame& frame, float dt) {
	static UInt32 s_linesLeft = kGlideLogLines;
	const bool wasFollowing = anchor.following;
	const vr::openvr::HmdMatrix34 before = anchor.head;
	vr::StepViewAnchor(anchor, shown, frame.haveHead, frame.head, settings.viewFollowDegrees,
	                   settings.viewLockedToHead, settings.viewFollowSeconds, dt, settings.viewFollowsPitch);
	frame.viewAnchorValid[row] = anchor.valid;
	frame.viewAnchor[row] = anchor.head;
	if (wasFollowing || !anchor.following || s_linesLeft == 0) {
		return;
	}
	--s_linesLeft;
	const float heading = vr::HeadingApartDegrees(before, frame.head);
	const float pitch = vr::HeadPitchDegrees(frame.head) - vr::HeadPitchDegrees(before);
	const float pitchApart = pitch < 0.0f ? -pitch : pitch;
	OBVR_LOG("HandHud: the %s row starts to glide after the view (%s, pitch %s): %.1f deg apart, heading %.1f, "
	         "pitch %.1f (the row at %.1f, the view at %.1f), from %.0f deg",
	         row == 0 ? "top" : "bottom", Cause(heading, pitchApart),
	         settings.viewFollowsPitch ? "followed" : "ignored",
	         static_cast<double>(vr::ViewDirectionApartDegrees(before, frame.head)), static_cast<double>(heading),
	         static_cast<double>(pitchApart), static_cast<double>(vr::HeadPitchDegrees(before)),
	         static_cast<double>(vr::HeadPitchDegrees(frame.head)), static_cast<double>(settings.viewFollowDegrees));
}

}  // namespace obvr::camera
