#pragma once

// How far ahead to predict the poses when the frame's wait for them comes late
// ([Render] LatePoseWait, docs/late-pose-wait.md): the camera hook takes the
// head's and the controllers' poses without waiting
// (GetDeviceToAbsoluteTrackingPose, predicted some seconds ahead), the
// engine's update runs on them, and WaitGetPoses - the frame's one block -
// comes at the render's entry, so the update's work lands in the wait instead
// of pushing the frame's present out (OBNG's measurement of 2026-11-20: a
// cell crossing's grid move after the wait cost a period at 36 of 40
// crossings; the menus' opening, docs/inventory-tablet-spec.md 13).
//
// The poses have to be predicted to the moment WaitGetPoses would have
// predicted them to: when the frame is lit. Its wait ends a running start
// before the vsync it is drawn for (the compositor's newPosesReadyMs, about
// 3 ms); the frame is shown at the vsync after that one, and lit the display's
// vsync-to-photons later (Prop_SecondsFromVsyncToPhotons_Float). About 25 ms at
// 90 Hz, as Valve said of WaitGetPoses' own (openvr issue 518). The wait this
// frame will end on is the last one's plus whole periods - the first beat not
// already past. Pure; covered by pose_prediction_test.

namespace obvr::vr {

// Seconds from `now` to the photons of the frame being updated. All times in
// seconds on one clock; `runningStart` is how long before its vsync a wait
// ends, `vsyncToPhotons` the display's own. Negative when it cannot be said
// (no wait yet, no period): the caller then waits at once, as before.
inline double SecondsToPhotons(double now, double lastWaitEnd, double period, double runningStart,
                               double vsyncToPhotons) {
	if (!(period > 0.0) || !(lastWaitEnd > 0.0) || now < lastWaitEnd) {
		return -1.0;
	}
	// The wait this frame will end on: the first beat after the last wait's
	// that is not already past. A frame late by more than a few periods is
	// a load, not a beat to keep - predict as if the next one is it.
	double waitEnd = lastWaitEnd + period;
	for (int i = 0; i < 8 && waitEnd < now; ++i) {
		waitEnd += period;
	}
	if (waitEnd < now) {
		waitEnd = now;
	}
	const double ahead = runningStart > 0.0 ? runningStart : 0.0;
	const double photons = vsyncToPhotons > 0.0 ? vsyncToPhotons : 0.0;
	return (waitEnd - now) + ahead + period + photons;
}

}  // namespace obvr::vr
