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

## Measured (2026-11-20, harness without SteamVR, --exclusive)

**Cell crossings** (OBNG's rides, obng-profile-ride-anvil, stand-in 75 Hz,
10 crossings a ride, OBNG's tools/re/crossing-slack.py):

| Ride | Crossings without a lost period | Frames over 20 ms at crossings | Frames over 14.5 ms (all / away from crossings) | p99 |
|---|---|---|---|---|
| both off, 20261120-145414 | 1 of 10 | 13 | 125 / 72 | 14.8 ms |
| OBNG's GridMoveDefer, -144725 / -145739 | 6, 5 | 10, 6 | 119 / 70, 121 / 68 | 14.8 ms |
| LatePoseWait, 20261120-145049 / -150104 | 6, 7 | 8, 5 | 47 / 17, 42 / 20 | 14.1 ms |

Both clear the crossings about equally; the late wait also takes two thirds
of all frames over 14.5 ms away, because every small overrun of the update
now meets the wait. GridMoveDefer stays off (OBNG's fallback). The crossings
that still lose a period have 9.8-14.7 ms of update before the wait - more
than the 8.7 ms of slack; the grid's move itself is OBNG's next lever.
OBNG's split of a ride with the late wait on (obng-profile-ride-anvil-late,
15:31, the update step's callees timed; 18 steps that missed a period in
119 s): the grid's move 7.0-11.0 ms in 4 (moves of 4-6.5 ms now fit), the IO
pump 0x00433590 8.3-15.4 ms in 6 (task ends that are not cell unloads),
0x004402F0 8.1-25.0 ms in 3, the sound update 0x006AE860 once, the actors'
ProcessManager in 2, the render alone in 1-2 - all the engine's, none of
OBVR's. With the late wait the crossings are no longer the main group.

**The menus' opening** (tablet-open-hitch-late.txt against
tablet-open-hitch.txt, the same DLL, 20261120-150502 and -150732): no
change. The long frame on opening is 17-28 ms either way; its cost is the 2D
pass that draws the newly opened menu (5-6 ms of hud_between), which comes
after the wait in both orders - the render's own work, which no place of the
wait can take in.

## Not known yet

- The prediction on a real headset: the stand-in does not move the head
  between the two calls and does not say its vsync-to-photons (taken as 0
  there). On SteamVR the log says how far the late wait's head was from the
  predicted one, in mm and degrees.
- How the reprojection from the submitted pose looks - the headset's.

## Files

`vr/PosePrediction.h` (pure), `vr/OpenVRFramePoses.cpp` (WaitGetPoses moved
here, PredictFramePoses, the log), `render/HeadsetFramePoses.cpp` (the wait
split out of BeginFrame, BeginFrameEarly, WaitLate), `camera/CameraHook.cpp`
(the three calls), `[Render] LatePoseWait` in OBVR.ini (default 0).
