# The late pose wait (`[Render] LatePoseWait`)

Asked (Nadi, 2026-11-20): "go obvr weg" - move OBVR's wait for the headset's
poses from the camera update to just before the render, so the engine's work
in a frame lands in the time the wait spends anyway.

## Why

Each frame OBVR waits once for the compositor (`WaitGetPoses`): it blocks
until the headset wants the next frame and hands back the poses to draw it
with. Until now the wait sat in the camera hook (0x0066BE6E), early in the
engine's update step. OBNG measured the order (2026-11-20, stand-in 75 Hz):
the IO pump, then OBVR's wait, then the grid's move (0x00444FB0), then the
render and the present. In a normal frame the wait takes about 8.9 ms of the
13.3, so work before it is free and work after it pushes the present out.

- A cell crossing's grid move (4.5 to 7.5 ms, up to 15.8) runs after the
  wait: the frame is late and the next misses its beat - one lost period at
  36 of 40 crossings. Derived by OBNG: with the wait after the move, 26 of
  the 40 would lose none.
- Opening a menu: 2.5 to 4.7 ms of the engine's work after the wait, and a
  2D pass of up to 5.6 ms (docs/inventory-tablet-spec.md 13).

## How

With `LatePoseWait=1`:

1. **The camera hook** (`HeadsetRenderer::BeginFrameEarly`) opens the frame
   without waiting. It predicts every device's pose to the moment the frame
   will be lit (`GetDeviceToAbsoluteTrackingPose` in the seated universe the
   compositor uses) and keeps them as the frame's render poses. The head
   tracker, the hands (`vr/HandPoseSource.h`) and everything in the update
   read them as they read `WaitGetPoses`' before.
2. **How far ahead** (`vr/PosePrediction.h`, pose_prediction_test): to where
   `WaitGetPoses` would have predicted. The frame's wait will end on the
   first beat after the last wait's end that is not past. From there it is a
   running start to the vsync (the compositor's `newPosesReadyMs`, kept by
   the submit pacing), a period to the vsync the frame is shown at, and the
   display's vsync-to-photons (`Prop_SecondsFromVsyncToPhotons_Float`, 2001).
   About 25 ms at 90 Hz, as Valve says of `WaitGetPoses`' own.
3. **The wait** (`HeadsetRenderer::WaitLate`) comes at the render's entry -
   `ScenePassWanted`, after the update and the hands' work before the first
   pass - or at Present for a frame with no world render. The pacing after it
   (submit past the vsync, the half-rate watch, the compositor's timing) is
   unchanged. Its own poses are not used for drawing; they are compared with
   the predicted ones for the log ("Poses: the late wait's head was ... mm
   and ... degrees from the one predicted before it").
4. **The submit** carries the pose the frame was drawn with
   (`Submit_TextureWithPose`, `OpenVRBackend::SubmitEye`). Valve: the
   compositor reprojects against `WaitGetPoses`' poses unless told otherwise
   (openvr issue 518); told, it corrects from the predicted one.

Falls back to the wait at the camera hook when it cannot predict: no wait yet
(the first frame), no display rate, no valid head pose.

What stays as it was: the menu stand-in (`PrepareMenuFrameIfNeeded`) already
waits at the render's entry on menu frames; the present path's own wait for
a menu delivered flat; the alternate-eyes and held pictures, which submit
with their own poses.

## Not known yet

- Whether it gains what OBNG derived: measured by OBNG's crossing rides and
  tablet-open-hitch-late.txt.
- The prediction's error on a real headset (the stand-in's poses do not move
  between the two calls) and how the reprojection from the submitted pose
  looks - the headset's.
- The stand-in does not report vsync-to-photons; it predicts the beats
  alone there.

## Files

`vr/PosePrediction.h` (pure), `vr/OpenVRFramePoses.cpp` (WaitGetPoses moved
here, PredictFramePoses, the log), `render/HeadsetFramePoses.cpp` (the wait
split out of BeginFrame, BeginFrameEarly, WaitLate), `camera/CameraHook.cpp`
(the three calls), `[Render] LatePoseWait` in OBVR.ini (default 0).
