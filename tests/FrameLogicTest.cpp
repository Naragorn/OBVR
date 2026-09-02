// Checks the per-frame bookkeeping of the camera hook.
//
// The callback these belong to cannot be tested - it needs a live CameraNode
// and reads the player through a hard-coded address - so the decisions it makes
// are kept in FrameLogic.h where they can be. All three are small enough to
// look obviously correct, which is exactly why they are worth pinning down: the
// recenter edge and the point-of-view transition were each written once, read
// once, and would break silently. A recenter that fires every frame while the
// key is held would re-zero the view continuously, and the only symptom would
// be a camera that refuses to move.
//
// No Windows API here, so this one builds and runs on Linux as well.

#include <cmath>
#include <cstdio>
#include <limits>

#include "camera/FrameLogic.h"
#include "core/MathFns.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestKeyEdge() {
	std::printf("Recenter key edge\n");

	obvr::camera::KeyEdge edge;

	Check(!edge.IsDown(), "a fresh edge starts up");
	Check(!edge.Update(false), "a key that stays up is no edge");
	Check(!edge.Update(false), "and still is not on the next frame");

	Check(edge.Update(true), "pressing the key is an edge");
	Check(edge.IsDown(), "and the key is remembered as down");

	// The one that matters. At 60 frames per second a held key would otherwise
	// recenter 60 times a second, taking the current pose as the new zero every
	// time - the camera would appear frozen for as long as the key is held.
	Check(!edge.Update(true), "holding it is not an edge again");
	Check(!edge.Update(true), "however long it is held");

	Check(!edge.Update(false), "releasing it is not an edge");
	Check(!edge.IsDown(), "and the key is remembered as up");

	Check(edge.Update(true), "pressing it again is an edge");
}

void TestKeyEdgeReset() {
	std::printf("Recenter switched off while the key is held\n");

	// RecenterKey=0 in the INI takes this path, and the INI can be reloaded
	// while the game runs. Without the reset, a key held across the change
	// would count as still down and the next poll would see no edge - the first
	// press after switching the feature back on would be swallowed.
	obvr::camera::KeyEdge edge;

	Check(edge.Update(true), "the key goes down");
	edge.Reset();
	Check(!edge.IsDown(), "reset forgets that it was down");
	Check(edge.Update(true), "so the next press counts as an edge again");
}

void TestPointOfView() {
	std::printf("Point of view\n");

	using obvr::camera::PovEvent;
	obvr::camera::State state;

	Check(!state.sawCameraNode, "nothing has been seen yet");

	Check(state.ObservePointOfView(false) == PovEvent::FirstPass,
	      "the first pass reports itself as the first pass");
	Check(state.sawCameraNode, "and is recorded");
	Check(!state.isThirdPerson, "with the point of view it saw");

	Check(state.ObservePointOfView(false) == PovEvent::Unchanged,
	      "the same point of view again is no event");

	Check(state.ObservePointOfView(true) == PovEvent::Switched,
	      "a change is reported");
	Check(state.isThirdPerson, "and the new point of view is recorded");

	Check(state.ObservePointOfView(true) == PovEvent::Unchanged,
	      "staying in third person is no event");
	Check(state.ObservePointOfView(false) == PovEvent::Switched,
	      "switching back is reported as well");
	Check(!state.isThirdPerson, "and recorded");
}

void TestFirstPassInThirdPerson() {
	std::printf("First pass in third person\n");

	// The default of the field is first person, so a game loaded in third
	// person has to be reported as a first pass and not as a switch - there
	// was nothing to switch from.
	using obvr::camera::PovEvent;
	obvr::camera::State state;

	Check(state.ObservePointOfView(true) == PovEvent::FirstPass,
	      "starting in third person is a first pass, not a switch");
	Check(state.isThirdPerson, "and the point of view is taken over");
	Check(state.ObservePointOfView(true) == PovEvent::Unchanged,
	      "the frame after it is quiet");
}

void TestIsDue() {
	std::printf("Periodic actions\n");

	using obvr::camera::IsDue;

	// 0 means off. Reaching the modulo with it would divide by zero, which is
	// the whole reason this is a function rather than an expression.
	Check(!IsDue(0, 0), "an interval of 0 is never due");
	Check(!IsDue(1, 0), "not on any frame");
	Check(!IsDue(120, 0), "however far the game has run");

	Check(IsDue(1, 1), "an interval of 1 is due every frame");
	Check(IsDue(2, 1), "really every frame");

	Check(!IsDue(119, 120), "an interval of 120 is not due at 119");
	Check(IsDue(120, 120), "is due at 120");
	Check(!IsDue(121, 120), "is not due at 121");
	Check(IsDue(240, 120), "and is due again at 240");

	// The callback counts from 1, so this case never occurs there. It is
	// checked anyway because the function is written to be usable on its own,
	// and a caller counting from 0 would get an action on its very first frame.
	Check(IsDue(0, 120), "frame 0 counts as due, which is why counting starts at 1");
}

void TestEyeAlternation() {
	std::printf("Which eye a frame belongs to\n");

	// Two places have to agree: the camera hook moves the camera to an eye,
	// and the renderer gives the finished frame to that eye. If they ever
	// disagreed the wearer would see each eye showing the other's viewpoint,
	// which is worse than no depth at all - so both ask this one function
	// rather than each deciding what "every other frame" means.
	Check(obvr::camera::IsLeftEyeFrame(0), "frame 0 is the left eye");
	Check(!obvr::camera::IsLeftEyeFrame(1), "frame 1 is the right");
	Check(obvr::camera::IsLeftEyeFrame(2), "and it alternates");

	// It has to keep alternating, including where a counter would wrap. A
	// frame count is 32 bits and a long session reaches large numbers.
	bool alternates = true;
	for (UInt32 frame = 0; frame < 1000; ++frame) {
		if (obvr::camera::IsLeftEyeFrame(frame) == obvr::camera::IsLeftEyeFrame(frame + 1)) {
			alternates = false;
			break;
		}
	}
	Check(alternates, "no two consecutive frames go to the same eye");

	// Across the wrap, where an implementation using division or a signed
	// counter would stumble.
	Check(obvr::camera::IsLeftEyeFrame(0xFFFFFFFEu) !=
	          obvr::camera::IsLeftEyeFrame(0xFFFFFFFFu),
	      "it still alternates at the top of the counter");
	Check(obvr::camera::IsLeftEyeFrame(0xFFFFFFFFu) != obvr::camera::IsLeftEyeFrame(0u),
	      "and across the wrap to zero");
}


void TestScaledEyeSeparation() {
	std::printf("Eye separation scaling\n");

	using obvr::camera::kMaxEyeSeparationScale;
	using obvr::camera::kMinEyeSeparationScale;
	using obvr::camera::ScaledEyeHalfSeparation;

	// 2.17 Oblivion units is what the 62 mm headset this was built against
	// reports as half its separation, so the numbers below read as the real
	// thing rather than as round inventions.
	constexpr float kHalf = 2.17f;

	// The default. Exactly the figure the tracker reported, untouched - the
	// geometrically true case almost every session runs in.
	Check(ScaledEyeHalfSeparation(kHalf, 1.0f) == kHalf, "a scale of 1 changes nothing");

	// The depth boost this exists for, and the flattening below 1. Both are
	// plain multiplication once the value has passed the checks.
	Check(ScaledEyeHalfSeparation(kHalf, 2.0f) == kHalf * 2.0f, "a scale of 2 doubles it");
	Check(ScaledEyeHalfSeparation(kHalf, 1.1f) == kHalf * 1.1f,
	      "the popular mild boost multiplies through");
	Check(ScaledEyeHalfSeparation(kHalf, 0.5f) == kHalf * 0.5f,
	      "below 1 flattens rather than being rejected");

	// The clamps. A positive number is a deliberate choice, so it is pulled to
	// the nearest sane value rather than ignored.
	Check(ScaledEyeHalfSeparation(kHalf, 0.1f) == kHalf * kMinEyeSeparationScale,
	      "a tiny positive scale is clamped up to the floor");
	Check(ScaledEyeHalfSeparation(kHalf, 10.0f) == kHalf * kMaxEyeSeparationScale,
	      "a huge scale is clamped down to the ceiling");
	Check(ScaledEyeHalfSeparation(kHalf, std::numeric_limits<float>::infinity()) ==
	          kHalf * kMaxEyeSeparationScale,
	      "infinity is just a huge scale and gets the ceiling");

	// The values nobody can have meant. Zero would stack both eyes in one
	// place, a negative scale would cross them, and NaN is what arithmetic on
	// garbage produces - all of them keep the measured truth instead. Zero is
	// also what ReadFloat returns for a word in the INI, so a typo lands here
	// and not on the floor clamp.
	Check(ScaledEyeHalfSeparation(kHalf, 0.0f) == kHalf, "zero falls back to the truth");
	Check(ScaledEyeHalfSeparation(kHalf, -1.0f) == kHalf, "a negative scale falls back too");
	Check(ScaledEyeHalfSeparation(kHalf, -std::numeric_limits<float>::infinity()) == kHalf,
	      "negative infinity likewise");
	Check(ScaledEyeHalfSeparation(kHalf, std::numeric_limits<float>::quiet_NaN()) == kHalf,
	      "NaN falls back to the truth rather than poisoning the offset");

	// No headset yet means no separation to scale. The caller's fallback is
	// the flat picture, and no multiplier may invent depth out of it.
	Check(ScaledEyeHalfSeparation(0.0f, 2.0f) == 0.0f,
	      "a separation of 0 stays 0 whatever the scale");
}

void TestBackBufferEye() {
	std::printf("Which eye the back buffer's picture belongs to\n");

	using obvr::camera::BackBufferEyeIsLeft;

	// From the camera hook, which runs before the frame is drawn. The back
	// buffer holds the previous frame, drawn from the previous camera
	// position, which under alternate eyes is the other eye.
	Check(!BackBufferEyeIsLeft(true, false),
	      "before drawing, a left-eye frame finds the right eye's picture waiting");
	Check(BackBufferEyeIsLeft(false, false), "and a right-eye frame finds the left eye's");

	// From a hook at the end of the frame, where the picture is the one just
	// drawn.
	Check(BackBufferEyeIsLeft(true, true), "after drawing, a left-eye frame holds the left");
	Check(!BackBufferEyeIsLeft(false, true), "and a right-eye frame holds the right");

	// The whole point of the function, said as one property: moving the
	// submit from one end of the frame to the other inverts the answer. That
	// is why this is a named function rather than an expression at the call
	// site - the expression was written once, and when the submit moved it
	// would have kept quietly meaning the opposite.
	//
	// The failure is not a lost depth cue but an inverted one: each eye shown
	// the other eye's viewpoint, which the eyes cannot fuse. It was reported
	// from a headset as a picture that would not hold still.
	for (int eye = 0; eye < 2; ++eye) {
		const bool isLeft = eye == 0;
		Check(BackBufferEyeIsLeft(isLeft, true) != BackBufferEyeIsLeft(isLeft, false),
		      "moving the submit across the frame inverts which eye the picture is for");
	}
}

void TestFrameClock() {
	std::printf("Frame clock\n");

	// A counter that ticks a million times a second, so one tick is one
	// microsecond and every number below reads directly as a duration.
	constexpr long long kPerSecond = 1000000;
	obvr::camera::FrameClock clock;

	// Nothing to measure against on the first call. Returning some invented
	// frame time would be worse than saying so: the caller treats 0 as "no
	// timing" and skips smoothing for that one frame.
	Check(clock.Tick(5000000, kPerSecond) == 0.0f, "the first reading reports no frame time");

	const float sixtyFps = clock.Tick(5000000 + 16667, kPerSecond);
	Check(sixtyFps > 0.0166f && sixtyFps < 0.0167f, "16667 microseconds is about a 60 fps frame");

	const float thirtyFps = clock.Tick(5000000 + 16667 + 33333, kPerSecond);
	Check(thirtyFps > 0.0333f && thirtyFps < 0.0334f, "and 33333 is about a 30 fps one");

	// A frequency of 0 would divide by zero. It should not happen, but the
	// value comes from an API call rather than from OBVR.
	Check(clock.Tick(6000000, 0) == 0.0f, "a frequency of 0 reports no frame time");
}

void TestFrameClockGaps() {
	std::printf("Frame clock, pauses and oddities\n");

	constexpr long long kPerSecond = 1000000;
	obvr::camera::FrameClock clock;
	clock.Tick(0, kPerSecond);

	// A loading screen, an alt-tab or a breakpoint. Fed unclamped into an
	// exponential approach, a gap of seconds covers the whole remaining
	// distance in one step - so the smoothing would vanish precisely on the
	// first frame back, which is the one frame where a jump is most visible.
	const float afterPause = clock.Tick(30 * kPerSecond, kPerSecond);
	Check(afterPause == obvr::camera::FrameClock::kMaxDeltaSeconds,
	      "a pause of 30 seconds is reported as the maximum, not as 30 seconds");

	// Recovery has to be immediate: the frame after the pause is measured
	// against the pause, not against the frame before it.
	const float next = clock.Tick(30 * kPerSecond + 16667, kPerSecond);
	Check(next > 0.0166f && next < 0.0167f, "the frame after a pause is measured normally");

	// The counter is monotonic, so this means a stale reading rather than
	// time running backwards. A negative frame time would run the smoothing
	// the wrong way.
	Check(clock.Tick(0, kPerSecond) == 0.0f, "a reading that went backwards reports nothing");

	// Two frames so close together that the counter has not moved.
	clock.Tick(40 * kPerSecond, kPerSecond);
	Check(clock.Tick(40 * kPerSecond, kPerSecond) == 0.0f,
	      "two readings at the same tick report nothing");

	// After a reset the next call is a first call again.
	obvr::camera::FrameClock reset;
	reset.Tick(1000, kPerSecond);
	reset.Reset();
	Check(reset.Tick(2000, kPerSecond) == 0.0f, "after a reset the next reading is a first one");
}

}  // namespace

void TestDeliverFrame() {
	std::printf("How a frame reaches the headset\n");

	using obvr::camera::DeliverFrame;
	using obvr::camera::FrameDelivery;

	using obvr::camera::kWorldlessBridgeFrames;

	// Nothing drew the world for long enough that this is a presentation, not
	// a stray frame. Videos, loading screens and the main menu are this case,
	// and the menu setting must not reach them: there is no world behind them
	// for a menu to hang in front of.
	for (int world = 0; world < 2; ++world) {
		for (int held = 0; held < 2; ++held) {
			Check(DeliverFrame(false, false, world != 0, held != 0,
			                   kWorldlessBridgeFrames) == FrameDelivery::Cinema,
			      "a sustained flat presentation is the cinema screen, whatever the setting");
		}
	}

	// The world is being drawn and nothing is in front of it.
	for (int world = 0; world < 2; ++world) {
		Check(DeliverFrame(true, false, world != 0, true, 0) == FrameDelivery::Stereo,
		      "a camera pass with no menu is stereo, whatever the setting");
	}

	// Menus=cinema, which is what OBVR has always done and stays the default.
	// The case the old flicker came from: Oblivion keeps drawing the world
	// behind an open menu but not on every frame, so before the menu was asked
	// about, the delivery changed with the camera - full stereo world one
	// frame, a small flat rectangle in black the next.
	Check(DeliverFrame(true, true, false, true, 0) == FrameDelivery::Cinema,
	      "a menu on the cinema screen stays there even when the world was drawn");
	Check(DeliverFrame(false, true, false, true, 0) == FrameDelivery::Cinema,
	      "and on the frames it was not");

	// Menus=world. The world is delivered in stereo and the menu reaches the
	// headset as its own overlay.
	Check(DeliverFrame(true, true, true, true, 0) == FrameDelivery::Stereo,
	      "a menu in the world is a stereo frame when the world was drawn");

	// The flow that makes it possible at all. Falling back to the cinema
	// screen here is the flicker, in its new clothes.
	Check(DeliverFrame(false, true, true, true, 0) == FrameDelivery::HeldStereo,
	      "and holds the last pair on the frames the world was not drawn");

	// Nothing captured yet - the main menu, or a menu opened before the first
	// dual frame. A picture beats the test pattern.
	Check(DeliverFrame(false, true, true, false, 0) == FrameDelivery::Cinema,
	      "with no pair to hold, the cinema screen is the only honest picture");

	// The properties, said once rather than case by case, with the streak past
	// the bridge so no stray-frame grace muddies them.
	for (int pass = 0; pass < 2; ++pass) {
		for (int menu = 0; menu < 2; ++menu) {
			for (int world = 0; world < 2; ++world) {
				for (int held = 0; held < 2; ++held) {
					const FrameDelivery delivery =
						DeliverFrame(pass != 0, menu != 0, world != 0, held != 0,
					                 kWorldlessBridgeFrames);

					// Holding requires something held. This is the one that
					// keeps the main menu off the held path.
					if (delivery == FrameDelivery::HeldStereo) {
						Check(held != 0, "a held delivery never happens with nothing held");
						Check(menu != 0 && world != 0,
						      "and only ever for a menu asked to hang in the world");
					}

					// The setting reaches menu frames and nothing else, so
					// turning it on cannot change what a video does.
					if (menu == 0) {
						Check(delivery == DeliverFrame(pass != 0, false, !(world != 0),
						                               held != 0, kWorldlessBridgeFrames),
						      "with no menu open the setting changes nothing");
					}
				}
			}
		}
	}
}

void TestWorldlessBridge() {
	std::printf("The stray frames with no world in them\n");

	using obvr::camera::DeliverFrame;
	using obvr::camera::FrameDelivery;
	using obvr::camera::kWorldlessBridgeFrames;

	// A stray frame with the world simply not drawn - the seam that closes a
	// menu, the gap a dialogue's exit transition leaves. By every other rule
	// that is the cinema screen, and it flashed up as exactly that: black bars
	// mid-close once, a split-second grey flash at every dialogue's end later.
	for (UInt32 streak = 0; streak < kWorldlessBridgeFrames; ++streak) {
		Check(DeliverFrame(false, false, true, true, streak) == FrameDelivery::HeldStereo,
		      "a stray worldless frame inside the bridge holds the pair");
	}

	// The bridge ends where a flat presentation begins - a video or a loading
	// screen reached this way must still be shown, not held over forever.
	Check(DeliverFrame(false, false, true, true, kWorldlessBridgeFrames) ==
	          FrameDelivery::Cinema,
	      "the frame past the bridge is the screen, so a video still shows");
	Check(DeliverFrame(false, false, true, true, kWorldlessBridgeFrames + 5) ==
	          FrameDelivery::Cinema,
	      "and it stays the screen from then on");

	// Nothing to hold: a stray falls back to the screen like any other frame.
	Check(DeliverFrame(false, false, true, false, 0) == FrameDelivery::Cinema,
	      "a stray with no pair held is still the screen");

	// The bridge does not reach a frame that drew a world - that is stereo on
	// its own merits and needs no help.
	Check(DeliverFrame(true, false, true, true, 0) == FrameDelivery::Stereo,
	      "a world render outranks the bridge");

	// Nor does it change a menu frame, which is decided before it.
	Check(DeliverFrame(false, true, true, true, kWorldlessBridgeFrames) ==
	          FrameDelivery::HeldStereo,
	      "a menu still held is held for its own reason, streak or no streak");
	Check(DeliverFrame(false, true, false, true, 0) == FrameDelivery::Cinema,
	      "and a menu on the cinema screen stays there, bridge or not");

	// The sequences as the game actually runs them, with the streak threaded
	// the way CameraHook threads it: read before the frame, advanced after.
	// First the menu close, then the dialogue exit, then a video - the two
	// bridged flows and the one that must not be.
	struct Step {
		bool cameraPass;
		bool menuIsUp;
		FrameDelivery expected;
		const char* what;
	};
	const Step steps[] = {
		{true, false, FrameDelivery::Stereo, "playing"},
		{true, true, FrameDelivery::Stereo, "the frame the menu opens on still drew a world"},
		{false, true, FrameDelivery::HeldStereo, "then the menu holds"},
		{false, true, FrameDelivery::HeldStereo, "and keeps holding"},
		{false, false, FrameDelivery::HeldStereo, "the closing seam holds rather than flashes"},
		{true, false, FrameDelivery::Stereo, "and the world is back"},
		{false, false, FrameDelivery::HeldStereo,
		 "a dialogue's exit gap is bridged rather than flashed"},
		{false, false, FrameDelivery::HeldStereo, "even two frames of it"},
		{true, false, FrameDelivery::Stereo, "and play resumes"},
		{false, false, FrameDelivery::HeldStereo, "a video's first frame is bridged"},
		{false, false, FrameDelivery::HeldStereo, "its second too"},
		{false, false, FrameDelivery::HeldStereo, "its third too"},
		{false, false, FrameDelivery::Cinema, "then the video takes the screen"},
		{false, false, FrameDelivery::Cinema, "and keeps it"},
	};
	UInt32 streak = 0;
	for (const Step& step : steps) {
		const FrameDelivery delivery =
			DeliverFrame(step.cameraPass, step.menuIsUp, true, true, streak);
		const bool worldless = !step.cameraPass && !step.menuIsUp;
		streak = worldless ? streak + 1 : 0;
		Check(delivery == step.expected, step.what);
	}
}

void TestMenuDressingWindow() {
	std::printf("Which held menus wear the pause dressing\n");

	using obvr::camera::kMenuDressingWindowFrames;
	using obvr::camera::MenuDressingWanted;

	// A pause menu stops the world on the spot: its held frames begin within
	// a frame or two of the menu opening.
	Check(MenuDressingWanted(0), "a menu holding on its opening frame is dressed");
	Check(MenuDressingWanted(1), "and one frame later");
	Check(MenuDressingWanted(kMenuDressingWindowFrames), "up to the window's edge");

	// A dialogue is also a menu, but its held frames are the exit fade, long
	// after the DialogMenu opened - dressing those painted the fade sepia,
	// seen as a washed-grey half second at the end of every conversation.
	Check(!MenuDressingWanted(kMenuDressingWindowFrames + 1),
	      "held frames past the window go undressed");
	Check(!MenuDressingWanted(3600), "a dialogue's exit fade, minutes in, goes undressed");
}

void TestMenuWorldProbe() {
	std::printf("When the menu-world probe runs its self-initiated render\n");

	using obvr::camera::FrameDelivery;
	using obvr::camera::kMenuWorldProbeAttempts;
	using obvr::camera::MenuWorldProbeWanted;

	// The flows that run: the probe is asked for, the engine's own render is
	// stopped - a held menu frame, or the cinema fallback a headless run
	// lands in when a sleeping headset never arms stereo - and the episode's
	// budget is not spent.
	Check(MenuWorldProbeWanted(true, FrameDelivery::HeldStereo, true, kMenuWorldProbeAttempts),
	      "a held menu frame with budget runs the probe");
	Check(MenuWorldProbeWanted(true, FrameDelivery::HeldStereo, true, 1),
	      "down to the last attempt");
	Check(MenuWorldProbeWanted(true, FrameDelivery::Cinema, true, kMenuWorldProbeAttempts),
	      "a cinema menu frame sits on the same stopped render - the headless case");

	// Off means off, whatever the frame looks like.
	Check(!MenuWorldProbeWanted(false, FrameDelivery::HeldStereo, true, kMenuWorldProbeAttempts),
	      "switched off, nothing runs");

	// A stereo frame's world was just drawn by the engine - there is nothing
	// to ask a self-initiated render.
	Check(!MenuWorldProbeWanted(true, FrameDelivery::Stereo, true, kMenuWorldProbeAttempts),
	      "a stereo frame has a world already");

	// A held frame can also be a bridged stray with no menu near it - the
	// seam that closes a menu, a dialogue's exit gap. Probing those would
	// draw over a back buffer mid-transition for a question about menus.
	Check(!MenuWorldProbeWanted(true, FrameDelivery::HeldStereo, false, kMenuWorldProbeAttempts),
	      "a bridged stray frame with no menu is left alone");

	// The budget is the difference between a measurement and a mechanism.
	Check(!MenuWorldProbeWanted(true, FrameDelivery::HeldStereo, true, 0),
	      "a spent budget ends the episode's probing");
}

void TestWorldControlProbe() {
	std::printf("When the probe runs its control on an ordinary world frame\n");

	using obvr::camera::WorldControlProbeWanted;

	// The one flow that runs: asked for, no menu, the engine drew this frame,
	// and the budget is not spent.
	Check(WorldControlProbeWanted(true, false, true, 3), "a world frame with budget runs it");
	Check(WorldControlProbeWanted(true, false, true, 1), "down to the last attempt");

	// Off means off.
	Check(!WorldControlProbeWanted(false, false, true, 3), "switched off, nothing runs");

	// A menu frame is the thing being controlled FOR, so it can never be the
	// control - that is the whole point of the pairing.
	Check(!WorldControlProbeWanted(true, true, true, 3),
	      "a menu frame is the measurement, not its control");

	// Without a camera pass the engine did not draw this frame either, so a
	// zero here would prove nothing about the moment the call is made from.
	Check(!WorldControlProbeWanted(true, false, false, 3),
	      "a frame the engine did not draw cannot control for one it did");

	// Each control costs an entire wasted world render.
	Check(!WorldControlProbeWanted(true, false, true, 0), "a spent budget stops it");
}

void TestMenuLiveBackground() {
	std::printf("When a menu frame needs OBVR to stand in for the camera pass\n");

	using obvr::camera::MenuFrameNeedsCameraStandIn;

	// The one flow that runs, and the persuasion minigame is measured to be
	// exactly it: dual pass, a menu is up, a headset is delivering poses, the
	// camera hook has left a base to build on, this frame has not armed one
	// already, and no camera pass ran - so the world the engine is drawing as
	// this is called would otherwise be discarded for a held still.
	Check(MenuFrameNeedsCameraStandIn(true, true, true, true, true, false, false),
	      "a menu frame the engine is drawing, with no camera pass, is armed");

	// Switched off from the INI. Not a matter of taste - this is the newest
	// code in the plugin, and taking it out of the picture without a rebuild
	// is what turns a bad session into a bisection.
	Check(!MenuFrameNeedsCameraStandIn(false, true, true, true, true, false, false),
	      "switched off, the held still comes back and nothing is armed");

	// Alternate eyes has no second capture to fill - it would submit one fresh
	// eye beside one stale one.
	Check(!MenuFrameNeedsCameraStandIn(true, false, true, true, true, false, false),
	      "only the dual pass captures a pair this way");

	// An ordinary world frame has a camera pass of its own.
	Check(!MenuFrameNeedsCameraStandIn(true, true, false, true, true, false, false),
	      "a frame with no menu needs no stand-in");

	// Without poses there is no head to draw from, and drawing from where the
	// head now is was the entire point.
	Check(!MenuFrameNeedsCameraStandIn(true, true, true, false, true, false, false),
	      "no headset, no pose to arm the frame with");

	// The main menu is exactly this: a menu before the first world render, so
	// no transform the game wrote has ever been seen.
	Check(!MenuFrameNeedsCameraStandIn(true, true, true, true, false, false, false),
	      "no camera base yet - the main menu - falls back");

	// The 2D pass runs more than once per frame; each entry would otherwise
	// arm its own frame.
	Check(!MenuFrameNeedsCameraStandIn(true, true, true, true, true, true, false),
	      "once per frame, however often the 2D pass runs");

	// A menu frame whose camera pass did run is already an ordinary stereo
	// frame. A dialogue is measured to be one, which is why dialogues always
	// looked right while the persuasion menu did not.
	Check(!MenuFrameNeedsCameraStandIn(true, true, true, true, true, false, true),
	      "the camera pass ran already - stand aside");
}

void TestStereoEyeStep() {
	std::printf("Where each eye sits, and how far apart they end up\n");

	using obvr::camera::EyeStep;
	using obvr::camera::StereoEyeStep;

	const auto Near = [](float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; };

	// Left first: the camera steps left, then the whole separation back right.
	const EyeStep left = StereoEyeStep(2.0f, true);
	Check(Near(left.toFirstEye, -2.0f), "the left eye sits left of centre");
	Check(Near(left.toSecondEye, 4.0f), "and the step to the right eye crosses the whole gap");

	// Right first is the mirror of it.
	const EyeStep right = StereoEyeStep(2.0f, false);
	Check(Near(right.toFirstEye, 2.0f), "the right eye sits right of centre");
	Check(Near(right.toSecondEye, -4.0f), "and the step to the left eye crosses back");

	// The invariant, and the reason this function exists at all. The second
	// eye must land exactly opposite the first - a copy of this arithmetic
	// with the shift's sign backwards put both eyes on the same side, the
	// second three half-separations out, and it was months before anything
	// switched that path on and showed it.
	Check(Near(left.toFirstEye + left.toSecondEye, -left.toFirstEye),
	      "left first: the second eye lands opposite the first");
	Check(Near(right.toFirstEye + right.toSecondEye, -right.toFirstEye),
	      "right first: the second eye lands opposite the first");

	// However far apart they are, they are that far apart both ways round.
	Check(Near(left.toSecondEye, -right.toSecondEye),
	      "the two orders are mirror images, not different separations");

	// No separation is a valid answer - it is what an eye scale of zero asks
	// for - and it must not become a step to somewhere else.
	const EyeStep none = StereoEyeStep(0.0f, true);
	Check(Near(none.toFirstEye, 0.0f) && Near(none.toSecondEye, 0.0f),
	      "no separation moves neither eye");
}

void TestCrosshair() {
	std::printf("Where the crosshair hangs, and how big it is there\n");

	using obvr::camera::CrosshairPlacement;
	using obvr::camera::CrosshairWanted;
	using obvr::camera::PlaceCrosshair;

	// Not called "near": windef.h defines that as an empty macro, and the
	// calls below then expand to nothing at all.
	const auto Near = [](float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; };

	using obvr::camera::CrosshairVisibility;

	// The plain case, spelled out once: switched on, the world was drawn, no
	// menu over it, and the "only when needed" option off.
	const auto Plain = [](bool enabled, bool worldFrame, bool menuIsUp) {
		CrosshairVisibility v;
		v.enabled = enabled;
		v.worldFrame = worldFrame;
		v.menuIsUp = menuIsUp;
		return v;
	};

	Check(CrosshairWanted(Plain(true, true, false)),
	      "on, on a world frame, the crosshair is shown");

	// Off is off - and off is the default, because Oblivion draws its own
	// unless bCrossHair is 0 and two crosshairs are worse than one.
	Check(!CrosshairWanted(Plain(false, true, false)), "switched off, nothing is shown");

	// The world was not redrawn: over a held picture an aiming point lies.
	Check(!CrosshairWanted(Plain(true, false, false)), "no world frame, no crosshair");
	Check(!CrosshairWanted(Plain(false, false, false)), "off and no world frame agree");

	// A menu is up. This is the flow the first version got wrong by folding it
	// into worldFrame: with Menus=world a dialogue is delivered in stereo, so
	// the world frame is real and the menu is real at the same time, and the
	// crosshair hung there through every conversation.
	Check(!CrosshairWanted(Plain(true, true, true)),
	      "a menu over a live world hides the crosshair");
	Check(!CrosshairWanted(Plain(true, false, true)), "a menu over a held world hides it too");
	Check(!CrosshairWanted(Plain(false, true, true)), "off and a menu agree");

	// ---- only when needed ---------------------------------------------------
	//
	// A crosshair is an aiming aid, and also a small bright thing permanently in
	// the middle of the view. This option keeps it out of the way until it is
	// doing its job.
	const auto Needed = [](bool thirdPerson, bool aimedAt, bool weapon) {
		CrosshairVisibility v;
		v.enabled = true;
		v.worldFrame = true;
		v.menuIsUp = false;
		v.onlyWhenNeeded = true;
		v.thirdPerson = thirdPerson;
		v.somethingAimedAt = aimedAt;
		v.weaponDrawn = weapon;
		return v;
	};

	Check(!CrosshairWanted(Needed(false, false, false)),
	      "walking around in first person with nothing to aim at hides it");
	Check(CrosshairWanted(Needed(false, true, false)),
	      "something activatable under it brings it back");
	Check(CrosshairWanted(Needed(false, false, true)), "so does drawing a weapon");
	Check(CrosshairWanted(Needed(false, true, true)), "and both at once, without arguing");

	// Third person listens to its OWN switch, not this one. The two views start
	// from opposite places: in first person the setting takes away a crosshair
	// the game always draws, in third person it governs one OBVR puts up in a
	// view Oblivion leaves empty.
	Check(CrosshairWanted(Needed(true, false, false)),
	      "third person ignores the first person switch");

	const auto NeededThird = [](bool aimedAt, bool weapon) {
		CrosshairVisibility v;
		v.enabled = true;
		v.worldFrame = true;
		v.menuIsUp = false;
		v.onlyWhenNeeded = false;
		v.onlyWhenNeededThirdPerson = true;
		v.thirdPerson = true;
		v.somethingAimedAt = aimedAt;
		v.weaponDrawn = weapon;
		return v;
	};

	Check(!CrosshairWanted(NeededThird(false, false)),
	      "with its own switch on, third person hides it too");
	Check(CrosshairWanted(NeededThird(true, false)), "and a target brings it back");
	Check(CrosshairWanted(NeededThird(false, true)), "and so does a drawn weapon");

	// The two switches really are independent - the flow that would break if
	// they were ever folded back into one.
	CrosshairVisibility firstOnly;
	firstOnly.enabled = true;
	firstOnly.worldFrame = true;
	firstOnly.onlyWhenNeeded = true;
	firstOnly.onlyWhenNeededThirdPerson = false;

	firstOnly.thirdPerson = false;
	Check(!CrosshairWanted(firstOnly), "restricted in first person");
	firstOnly.thirdPerson = true;
	Check(CrosshairWanted(firstOnly), "and unrestricted in third, from the same settings");

	CrosshairVisibility thirdOnly = firstOnly;
	thirdOnly.onlyWhenNeeded = false;
	thirdOnly.onlyWhenNeededThirdPerson = true;

	thirdOnly.thirdPerson = true;
	Check(!CrosshairWanted(thirdOnly), "restricted in third person");
	thirdOnly.thirdPerson = false;
	Check(CrosshairWanted(thirdOnly), "and unrestricted in first, the other way round");

	// The three original conditions still come first. Something aimed at during
	// a conversation must not put a crosshair over the dialogue.
	CrosshairVisibility duringMenu = Needed(false, true, true);
	duringMenu.menuIsUp = true;
	Check(!CrosshairWanted(duringMenu), "a menu still hides it, target or no target");

	CrosshairVisibility switchedOff = Needed(false, true, true);
	switchedOff.enabled = false;
	Check(!CrosshairWanted(switchedOff), "and switched off still means off");

	CrosshairVisibility heldFrame = Needed(true, true, true);
	heldFrame.worldFrame = false;
	Check(!CrosshairWanted(heldFrame), "and a held picture still means no crosshair");

	// Ordinary values pass through, and the width is the size at one metre
	// carried out to the distance: 0.025 at ten metres is a quarter of a metre
	// wide, which subtends the same angle as 0.025 at one.
	const CrosshairPlacement plain = PlaceCrosshair(10.0f, 0.025f);
	Check(Near(plain.distanceMetres, 10.0f), "a sane distance is left alone");
	Check(Near(plain.widthMetres, 0.25f), "the width is the size carried to the distance");

	// Closer than the eyes can comfortably converge on. Clamped, and the width
	// follows the clamped distance rather than the asked-for one - otherwise a
	// rejected distance would still change the size.
	const CrosshairPlacement tooNear = PlaceCrosshair(0.05f, 0.025f);
	Check(Near(tooNear.distanceMetres, 0.3f), "a distance inside the near limit is clamped");
	Check(Near(tooNear.widthMetres, 0.0075f), "the width follows the clamped distance");

	// Past the far end the depth stops meaning anything: the sight lines are
	// parallel long before this.
	const CrosshairPlacement tooFar = PlaceCrosshair(5000.0f, 0.025f);
	Check(Near(tooFar.distanceMetres, 100.0f), "a distance past the far limit is clamped");
	Check(Near(tooFar.widthMetres, 2.5f), "the width follows the far clamp too");

	// A size small enough to be invisible, and one large enough to be a wall.
	const CrosshairPlacement tiny = PlaceCrosshair(10.0f, 0.0f);
	Check(Near(tiny.widthMetres, 0.02f), "a vanishing size is clamped to something visible");

	const CrosshairPlacement huge = PlaceCrosshair(10.0f, 40.0f);
	Check(Near(huge.widthMetres, 5.0f), "an absurd size is clamped before it fills the view");

	// Both ends at once, which is the flow where one clamp could quietly undo
	// the other.
	const CrosshairPlacement both = PlaceCrosshair(-3.0f, 900.0f);
	Check(Near(both.distanceMetres, 0.3f) && Near(both.widthMetres, 0.15f),
	      "a nonsense pair clamps in both directions independently");
}

void TestAimTurnMode() {
	std::printf("When the body is turned to the gaze\n");

	using obvr::camera::AimTurnDue;
	using obvr::camera::AimTurnMode;

	// Arguments: mode, attackHeld, attackWasHeld, attackInProgress.

	// WhileAiming - the original. The control decides it, and nothing else.
	Check(AimTurnDue(AimTurnMode::WhileAiming, true, false, false),
	      "held is turning, in the original mode");
	Check(AimTurnDue(AimTurnMode::WhileAiming, true, true, false), "and stays turning while held");
	Check(!AimTurnDue(AimTurnMode::WhileAiming, false, true, false),
	      "and stops the moment it is let go");
	Check(!AimTurnDue(AimTurnMode::WhileAiming, false, false, true),
	      "the attack running does not extend it");

	// OnShot - the point of the mode. THE DRAW IS LEFT ALONE, which is what
	// keeps walking under the mouse's control while somebody aims anywhere they
	// like, for as long as they like.
	Check(!AimTurnDue(AimTurnMode::OnShot, true, false, false),
	      "drawing the bow does not turn the body");
	Check(!AimTurnDue(AimTurnMode::OnShot, true, true, false),
	      "and holding it drawn still does not");

	// The release, which is when the arrow starts being made.
	Check(AimTurnDue(AimTurnMode::OnShot, false, true, false),
	      "letting go turns the body for the shot");

	// And held through the attack, because the arrow is created some frames
	// after the release and leaves along the heading of that moment.
	Check(AimTurnDue(AimTurnMode::OnShot, false, false, true),
	      "and stays turned while the attack is still running");

	// Once the attack is over, nothing - which is what leaves nothing standing.
	Check(!AimTurnDue(AimTurnMode::OnShot, false, false, false),
	      "and lets go again once the shot is done");

	// Held wins over everything in OnShot: an attack still showing as in
	// progress while the control is down again is somebody starting the next
	// shot, and their draw should be as free as the last one.
	Check(!AimTurnDue(AimTurnMode::OnShot, true, false, true),
	      "a new draw is free even if the last attack is still finishing");
}

void TestAimReturn() {
	std::printf("Giving the aimed turn back when the shot goes\n");

	using obvr::camera::AimReturnWanted;
	using obvr::camera::kAimReturnLimitSeconds;

	// Arguments in order: enabled, headset, menuIsUp, attackHeld,
	// secondsSinceRelease, attackInProgress, bodyOffset.

	// THE FLOW THIS WAS REBUILT FOR. The control is released and the attack has
	// finished, so the arrow has gone and the heading is free to move again.
	Check(AimReturnWanted(true, true, false, false, 0.2f, false, true),
	      "once the shot is done, the body comes back round");

	// THE FAULT THE FIRST VERSION HAD, and the reason for the rebuild. Letting
	// go of the control STARTS the shot; the arrow spawns several frames later.
	// Straightening here would send it forwards instead of at what was aimed
	// at - breaking the aiming this whole feature serves.
	Check(!AimReturnWanted(true, true, false, false, 0.0f, true, true),
	      "the frame the control is released, the arrow has not left yet");
	Check(!AimReturnWanted(true, true, false, false, 0.1f, true, true),
	      "and it still has not a few frames later");

	// The safety limit, which is what keeps a wrong action value from disabling
	// the feature rather than merely delaying it. If the attack never appears
	// to end, the turn is still given back.
	Check(AimReturnWanted(true, true, false, false, kAimReturnLimitSeconds, true, true),
	      "an attack that never ends still lets go at the limit");
	Check(!AimReturnWanted(true, true, false, false, kAimReturnLimitSeconds - 0.1f, true, true),
	      "but not one moment before it");

	// Held is still aiming. The body is being handed the turn on these frames,
	// and taking it back at the same time would have the two fight.
	Check(!AimReturnWanted(true, true, false, true, 0.5f, false, true),
	      "still held, still aiming");
	Check(!AimReturnWanted(true, true, false, true, -1.0f, false, true),
	      "held with no clock running either");

	// A negative count is "not waiting for anything" - the control is held, or
	// this release was already settled. Without a value of its own, settled
	// would be indistinguishable from released-this-instant and the heading
	// would be written every frame for as long as nobody pressed anything.
	Check(!AimReturnWanted(true, true, false, false, -1.0f, false, true),
	      "nothing being waited for, nothing to do");

	// Nothing to give back. The ordinary case for nearly every frame.
	Check(!AimReturnWanted(true, true, false, false, 0.5f, false, false),
	      "no turn standing, nothing to do");

	// The third person's borrowed pitch counts as something standing on its
	// own - the caller folds it into the same flag, so a shot aimed with the
	// head level sideways is still given back.
	Check(AimReturnWanted(true, true, false, false, 0.5f, false, true),
	      "anything standing - a turn or a borrowed pitch - is given back");

	// Switched off, which is what somebody reaches for if the sickness comes
	// back. It has to actually stop it.
	Check(!AimReturnWanted(false, true, false, false, 0.5f, false, true),
	      "switched off, nothing happens");

	// No headset means the vanilla game, and OBVR does not touch the player's
	// heading there at all.
	Check(!AimReturnWanted(true, false, false, false, 0.5f, false, true),
	      "no headset, no interference");

	// Not into a paused world. The engine will not rebuild the camera from the
	// heading until play resumes, so the write and the compensation would sit
	// disagreeing until it did.
	Check(!AimReturnWanted(true, true, true, false, 0.5f, false, true),
	      "not while a menu is up");
}

void TestBorrowedCrosshair() {
	std::printf("When third person borrows the crosshair from first person\n");

	using obvr::camera::BorrowedCrosshairWanted;

	// The gap it fills: third person, switched on, nothing under the crosshair
	// and not sneaking. Oblivion draws no plain crosshair here, so the copy is
	// the only one there will be.
	Check(BorrowedCrosshairWanted(true, true, false, false),
	      "third person with nothing drawn borrows the crosshair");

	// THE TWO FLOWS THE FIRST VERSION GOT WRONG, both reported from the
	// headset. It pasted the copy in unconditionally, on the belief that
	// Oblivion draws nothing at all in the middle of the layer in third person.
	// It draws no plain crosshair - but it does draw these two.
	Check(!BorrowedCrosshairWanted(true, true, true, false),
	      "something aimed at is the game's icon, and the copy stays out of the way");
	Check(!BorrowedCrosshairWanted(true, true, false, true),
	      "sneaking is the game's eye, and the copy stays out of the way");
	Check(!BorrowedCrosshairWanted(true, true, true, true),
	      "and both at once, without arguing");

	// First person never borrows: it is where the crosshair comes FROM.
	Check(!BorrowedCrosshairWanted(false, true, false, false),
	      "first person has its own and never borrows");
	Check(!BorrowedCrosshairWanted(false, true, true, true), "whatever else is going on");

	// Switched off is off.
	Check(!BorrowedCrosshairWanted(true, false, false, false), "and off is off");
}

void TestCrosshairCutout() {
	std::printf("How much of the flat layer the crosshair takes with it\n");

	using obvr::camera::CrosshairSourcePixels;
	using obvr::camera::kCrosshairSourceLargestShare;
	using obvr::camera::kCrosshairSourceSmallestShare;

	// The measurement this replaced a constant with. A run at 5696x3164 lifted
	// a 96-pixel square - three per cent of the height, where the same 96 had
	// been over ten per cent on the ordinary picture it was chosen against. The
	// game's own crosshair had grown with the frame and the square had not,
	// which is why fragments were left behind while sneaking.
	Check(CrosshairSourcePixels(3164, 3.03f) == 94,
	      "the old fixed 96 was about three per cent of that reported height");

	// Six per cent of that same height is nearly twice the square.
	const UInt32 wide = CrosshairSourcePixels(3164, 6.0f);
	Check(wide == 188, "six per cent of 3164 is 188 pixels");
	Check(wide > 96 * 2 - 10, "which is roughly twice what was being lifted before");

	// THE POINT OF THE CHANGE: the same setting gives the same fraction of the
	// picture at any resolution, so a value found once stays right.
	//
	// Within a couple of pixels rather than exactly: the size is floored and
	// then pulled to an even number, so doubling the height gives 378 where
	// twice 188 is 376. That is rounding, not drift - the fraction is what is
	// being held fixed, and two pixels of a 380-pixel square is nothing.
	const auto NearPixels = [](UInt32 a, UInt32 b) { return a > b ? a - b <= 2 : b - a <= 2; };

	Check(NearPixels(CrosshairSourcePixels(1582, 6.0f), wide / 2),
	      "half the height, half the square - the share is what is fixed now");
	Check(NearPixels(CrosshairSourcePixels(6328, 6.0f), wide * 2),
	      "and twice the height, twice the square");

	// Even, because the square is centred by halving it. An odd size sits half a
	// pixel off centre, which is exactly how a one-pixel line gets left behind.
	for (UInt32 height = 1000; height < 1010; ++height) {
		Check((CrosshairSourcePixels(height, 6.0f) & 1u) == 0, "the square is always even");
	}

	// A height of zero means the size is not known yet, and the caller reads
	// zero as "lift nothing".
	Check(CrosshairSourcePixels(0, 6.0f) == 0, "an unknown height lifts nothing");

	// Both clamps. Below the low end the lift stops covering the crosshair;
	// above the high end it starts taking the name of whatever is being looked
	// at, which is drawn just underneath.
	Check(CrosshairSourcePixels(3164, 0.0f) ==
	          CrosshairSourcePixels(3164, kCrosshairSourceSmallestShare),
	      "a share of nothing is clamped to the smallest useful one");
	Check(CrosshairSourcePixels(3164, -5.0f) ==
	          CrosshairSourcePixels(3164, kCrosshairSourceSmallestShare),
	      "and a negative share with it");
	Check(CrosshairSourcePixels(3164, 90.0f) ==
	          CrosshairSourcePixels(3164, kCrosshairSourceLargestShare),
	      "an absurd share is clamped before it lifts half the screen");

	// NaN fails every comparison it appears in, so the clamp is written to
	// catch it rather than let it through into the rectangle.
	Check(CrosshairSourcePixels(3164, std::numeric_limits<float>::quiet_NaN()) ==
	          CrosshairSourcePixels(3164, kCrosshairSourceSmallestShare),
	      "a NaN share is clamped rather than passed on");

	// A tiny picture still lifts something. Zero is reserved for "not known".
	Check(CrosshairSourcePixels(40, 1.0f) >= 8, "a small picture still lifts a usable square");
	Check(CrosshairSourcePixels(40, 1.0f) != 0, "and specifically not nothing");
}

void TestCrosshairDepth() {
	std::printf("How deep the crosshair sits\n");

	using obvr::NiPoint3;
	using obvr::camera::CrosshairDepth;
	using obvr::camera::CrosshairDepthInput;

	const auto Near = [](float a, float b) { return a - b < 1e-3f && b - a < 1e-3f; };

	// Oblivion's own scale, and a camera at eye height above the origin looking
	// level along +Y. Every case below is built on this one arrangement so that
	// the numbers can be compared with each other.
	constexpr float kUnits = 69.99125f;
	const NiPoint3 eye{0.0f, 0.0f, 120.0f};
	const NiPoint3 level{0.0f, 1.0f, 0.0f};

	const auto Base = [&]() {
		CrosshairDepthInput input;
		input.haveTarget = true;
		input.cameraPosition = eye;
		input.gazeDirection = level;
		input.unitsPerMetre = kUnits;
		input.fallbackMetres = 3.0f;
		return input;
	};

	// Nothing under the crosshair. The ordinary case, not a failure - most of
	// what anyone looks at cannot be activated and records no reference.
	CrosshairDepthInput none = Base();
	none.haveTarget = false;
	none.targetPosition = NiPoint3{0.0f, 70.0f, 0.0f};
	Check(Near(CrosshairDepth(none), 3.0f), "no target falls back to the fixed distance");

	// THE CASE THIS FUNCTION EXISTS FOR. An actor standing one metre ahead: its
	// origin is between its feet, so it is 70 units along the gaze and 120
	// units below the eye. The straight line to that origin is 139 units, which
	// is 1.99 m - and placing the crosshair there while the eyes are converged
	// on a face one metre away would put back most of the doubling this whole
	// feature is meant to remove. The projection answers the metre.
	CrosshairDepthInput atFace = Base();
	atFace.targetPosition = NiPoint3{0.0f, 70.0f, 0.0f};
	const float faceDepth = CrosshairDepth(atFace);
	Check(Near(faceDepth, 1.0f), "an actor a metre ahead reads as a metre, not as its origin");
	Check(faceDepth < 1.5f, "and specifically not as the 1.99 m straight-line distance");

	// The same actor, looked at down at its feet. Now the gaze really does
	// point at the origin, and the projection agrees with the straight line -
	// which is what says the projection is not simply throwing height away.
	CrosshairDepthInput atFeet = Base();
	atFeet.targetPosition = NiPoint3{0.0f, 70.0f, 0.0f};
	atFeet.gazeDirection = NiPoint3{0.0f, 70.0f, -120.0f};

	// The expected value is computed rather than written down. A hand-typed
	// slant distance was wrong by 0.07 units the first time and still passed,
	// because the error happened to sit just inside the tolerance - which is
	// the exact way a test stops testing anything.
	const float slant = std::sqrt(70.0f * 70.0f + 120.0f * 120.0f) / kUnits;
	Check(Near(CrosshairDepth(atFeet), slant),
	      "looking down at the same feet gives the true slant distance");
	Check(slant > 1.9f, "which is nearly two metres, and so not the same answer as the face");

	// Sideways offset falls out the same way: something a metre ahead and half
	// a metre to the side is still a metre deep, because depth is measured
	// along the gaze and not to the object.
	CrosshairDepthInput beside = Base();
	beside.targetPosition = NiPoint3{35.0f, 70.0f, 0.0f};
	Check(Near(CrosshairDepth(beside), 1.0f), "a target off to one side keeps its depth");

	// Behind the camera. Reachable in third person, and for a frame whenever a
	// reference outlives the look that found it. A negative depth would hang
	// the quad behind the wearer's head.
	CrosshairDepthInput behind = Base();
	behind.targetPosition = NiPoint3{0.0f, -70.0f, 0.0f};
	Check(Near(CrosshairDepth(behind), 3.0f), "a target behind the camera falls back");

	// Exactly in the eye plane: a depth of zero is not a depth either.
	CrosshairDepthInput edgeOn = Base();
	edgeOn.targetPosition = NiPoint3{70.0f, 0.0f, 120.0f};
	Check(Near(CrosshairDepth(edgeOn), 3.0f), "a target level with the eyes falls back");

	// A gaze with no direction - an identity-shaped world transform on the
	// first frames of a load, before the scene graph has computed one.
	CrosshairDepthInput noGaze = Base();
	noGaze.targetPosition = NiPoint3{0.0f, 70.0f, 0.0f};
	noGaze.gazeDirection = NiPoint3{0.0f, 0.0f, 0.0f};
	Check(Near(CrosshairDepth(noGaze), 3.0f), "a gaze of no length falls back");

	// An unnormalised gaze must not scale the answer. This is the fault that
	// would be hardest to see from inside a headset: the crosshair would sit at
	// a plausible depth that is consistently wrong.
	CrosshairDepthInput longGaze = Base();
	longGaze.targetPosition = NiPoint3{0.0f, 70.0f, 0.0f};
	longGaze.gazeDirection = NiPoint3{0.0f, 5.0f, 0.0f};
	Check(Near(CrosshairDepth(longGaze), 1.0f), "a gaze five units long answers the same metre");

	// Units that are not a scale. The INI is written by hand, and dividing by
	// these would give a depth in nothing at all.
	CrosshairDepthInput zeroUnits = Base();
	zeroUnits.targetPosition = NiPoint3{0.0f, 70.0f, 0.0f};
	zeroUnits.unitsPerMetre = 0.0f;
	Check(Near(CrosshairDepth(zeroUnits), 3.0f), "units of zero fall back");

	CrosshairDepthInput negativeUnits = Base();
	negativeUnits.targetPosition = NiPoint3{0.0f, 70.0f, 0.0f};
	negativeUnits.unitsPerMetre = -70.0f;
	Check(Near(CrosshairDepth(negativeUnits), 3.0f), "negative units fall back");

	// The units really are applied, rather than the answer happening to be
	// right at Oblivion's scale.
	CrosshairDepthInput halfScale = Base();
	halfScale.targetPosition = NiPoint3{0.0f, 70.0f, 0.0f};
	halfScale.unitsPerMetre = 35.0f;
	Check(Near(CrosshairDepth(halfScale), 2.0f), "half the units per metre gives twice the depth");

	// The fallback is whatever the caller says, not a constant hidden in here.
	CrosshairDepthInput otherFallback = Base();
	otherFallback.haveTarget = false;
	otherFallback.fallbackMetres = 7.5f;
	Check(Near(CrosshairDepth(otherFallback), 7.5f), "the fallback is the caller's value");

	// The near end of the pick, where the vergence error is worst and this
	// feature earns its place: something at arm's length reads as arm's length.
	CrosshairDepthInput close = Base();
	close.targetPosition = NiPoint3{0.0f, 35.0f, 0.0f};
	Check(Near(CrosshairDepth(close), 0.5f), "half a metre ahead reads as half a metre");
}

void TestLayoutProbeDue() {
	std::printf("When the layout probe takes its next measurement\n");

	using obvr::camera::kLayoutProbeFrameGap;
	using obvr::camera::LayoutProbeDue;

	// The flow that measures: asked for, and the last measurement is at
	// least the gap ago.
	Check(LayoutProbeDue(true, kLayoutProbeFrameGap, 0),
	      "the gap has passed, so this frame measures");
	Check(LayoutProbeDue(true, 1000 + kLayoutProbeFrameGap, 1000),
	      "exactly at the gap counts as due");

	// Off means off, however long it has been.
	Check(!LayoutProbeDue(false, 100000, 0), "switched off, nothing measures");

	// Between measurements the probe waits - one readback stall every couple
	// of seconds, not one per frame.
	Check(!LayoutProbeDue(true, 1000 + kLayoutProbeFrameGap - 1, 1000),
	      "one frame short of the gap waits");

	// The frame counter wrapping around must not freeze the probe: the
	// difference arithmetic keeps working across the wrap.
	Check(LayoutProbeDue(true, kLayoutProbeFrameGap - 5, 0xFFFFFFFFu - 4),
	      "a wrapped frame counter still measures");
}

void TestMenusCanReachTheWorld() {
	std::printf("When a menu asked to hang in the world actually can\n");

	using obvr::camera::DeliverFrame;
	using obvr::camera::FrameDelivery;
	using obvr::camera::MenusCanReachTheWorld;

	Check(MenusCanReachTheWorld(true, true), "asked for, and the overlay is there to carry it");
	Check(!MenusCanReachTheWorld(false, true), "not asked for");

	// The fault this exists to prevent, and it is not a cosmetic one: the eyes
	// are captured before the 2D pass draws, so without the overlay the world
	// goes out in stereo with the menu neither in it nor in front of it. The
	// wearer would be looking at a menu that is simply not there, with nothing
	// on screen to say why.
	Check(!MenusCanReachTheWorld(true, false),
	      "asked for, but with no overlay the menu would arrive nowhere at all");
	Check(!MenusCanReachTheWorld(false, false), "neither");

	// And the consequence, said through the decision it feeds: with the
	// overlay off, a menu frame goes to the cinema screen, where the menu is
	// part of the picture and therefore visible.
	for (int pass = 0; pass < 2; ++pass) {
		Check(DeliverFrame(pass != 0, true, MenusCanReachTheWorld(true, false), true, 0) ==
		          FrameDelivery::Cinema,
		      "with the overlay off a menu falls back to the screen, where it can be seen");
	}
}

void TestWantsSecondScenePass() {
	std::printf("When the world render runs twice\n");

	using obvr::camera::DeliverFrame;
	using obvr::camera::FrameDelivery;
	using obvr::camera::WantsSecondScenePass;

	// A frame the compositor accepted, a camera the hook actually moved to an
	// eye, and no menu in front.
	Check(WantsSecondScenePass(true, true, false, false),
	      "an open frame with an armed camera and no menu draws twice");

	// Every other flow stays single-pass, each for its own reason.
	Check(!WantsSecondScenePass(false, true, false, false),
	      "no open frame means nobody is waiting for the pictures");
	Check(!WantsSecondScenePass(true, false, false, false),
	      "an unarmed camera has no second viewpoint to draw from");
	Check(!WantsSecondScenePass(true, true, true, false),
	      "a menu on the cinema screen ignores the captures, so a second pass is waste");
	Check(!WantsSecondScenePass(false, false, false, false), "neither open nor armed");
	Check(!WantsSecondScenePass(false, true, true, false), "menu up and no open frame");
	Check(!WantsSecondScenePass(true, false, true, false), "menu up and unarmed");
	Check(!WantsSecondScenePass(false, false, true, false), "all three against it");

	// A menu in the world is a stereo frame like any other, so it wants both
	// eyes - this is the flow that pays for the setting.
	Check(WantsSecondScenePass(true, true, true, true),
	      "a menu asked to hang in the world draws twice like any stereo frame");
	Check(!WantsSecondScenePass(false, true, true, true),
	      "but not on a frame nobody is waiting for");
	Check(!WantsSecondScenePass(true, false, true, true),
	      "and not with an unarmed camera");

	// The property that keeps the render and the delivery agreeing: whenever
	// this says draw twice, the frame is delivered as stereo - so the captures
	// are always submitted. A second pass for a cinema frame would be two
	// renders for a picture that ignores both.
	for (int menu = 0; menu < 2; ++menu) {
		for (int world = 0; world < 2; ++world) {
			const bool menuIsUp = menu != 0;
			const bool menusInWorld = world != 0;
			if (WantsSecondScenePass(true, true, menuIsUp, menusInWorld)) {
				Check(DeliverFrame(true, menuIsUp, menusInWorld, true, 0) ==
				          FrameDelivery::Stereo,
				      "a frame that draws twice is always delivered as stereo");
			}
		}
	}
}

void TestFirstPassDrawsLeftEye() {
	std::printf("Which eye the first world render draws\n");

	using obvr::camera::FirstPassDrawsLeftEye;

	// Both flows, because the whole value of the switch is that the three
	// places reading it move together - and a constant that only ever
	// returned one answer would look exactly like a working switch until
	// somebody swapped the order and found the pair unchanged.
	Check(FirstPassDrawsLeftEye(false), "the left eye is drawn first by default");
	Check(!FirstPassDrawsLeftEye(true), "swapping the order draws the right eye first");
}

void TestWantsHudRedirect() {
	std::printf("When the 2D pass leaves the frame\n");

	using obvr::camera::DeliverFrame;
	using obvr::camera::FrameDelivery;
	using obvr::camera::WantsHudRedirect;

	// The three deliveries, each said once.
	Check(WantsHudRedirect(FrameDelivery::Stereo),
	      "a stereo frame sends the layer to its own texture, because the overlay shows it");
	Check(WantsHudRedirect(FrameDelivery::HeldStereo),
	      "so does a held frame - the overlay is exactly what is still live on those");
	Check(!WantsHudRedirect(FrameDelivery::Cinema),
	      "the cinema screen shows the frame itself, so the layer has to stay in it");

	// Now the same question asked the way the game asks it, over every input
	// the decision has - camera pass, menu, setting, and whether a pair is held.
	//
	// That last one is why this loop is written out rather than trusted. It was
	// pinned to true in an earlier version of this test, and the case it left
	// out is the main menu: a menu, no camera pass, no captured pair. That
	// falls back to the cinema screen while the redirect went on taking the
	// layer away, so the title screen had its menu in neither place and the
	// game looked hung. Sixteen combinations cost nothing; the one that was
	// missing cost a session.
	for (int bits = 0; bits < 16; ++bits) {
		const bool hadCameraPass = (bits & 1) != 0;
		const bool menuIsUp = (bits & 2) != 0;
		const bool menusInWorld = (bits & 4) != 0;
		const bool haveHeldEyes = (bits & 8) != 0;

		const FrameDelivery delivery =
			DeliverFrame(hadCameraPass, menuIsUp, menusInWorld, haveHeldEyes, 0);

		// The invariant, both ways round: the layer leaves the frame if and
		// only if something other than the frame is being shown. A menu that
		// is in neither place is what breaking it looks like.
		Check(WantsHudRedirect(delivery) == (delivery != FrameDelivery::Cinema),
		      "the layer leaves the frame exactly when the frame is not what is shown");
	}

	// The main menu on its own, named rather than left inside the loop, because
	// it is the flow that broke and the one a future change would break again.
	Check(DeliverFrame(false, true, true, false, 0) == FrameDelivery::Cinema,
	      "the main menu asked to hang in the world has no pair, so it takes the screen");
	Check(!WantsHudRedirect(DeliverFrame(false, true, true, false, 0)),
	      "and therefore keeps its layer, which is the only copy of it there is");

	// The in-game menu, which is the case the setting exists for: no camera
	// pass either, but a pair is held, so the overlay is live and the layer
	// should leave.
	Check(DeliverFrame(false, true, true, true, 0) == FrameDelivery::HeldStereo,
	      "an in-game menu in the world is held rather than screened");
	Check(WantsHudRedirect(DeliverFrame(false, true, true, true, 0)),
	      "so its layer leaves the frame and arrives as the overlay");
}

void TestSweepProbeStage() {
	std::printf("When the probe walks its own rungs\n");

	using obvr::camera::kProbeSinglePass;
	using obvr::camera::kProbeSweep;
	using obvr::camera::kSweepFirstBand;
	using obvr::camera::kSweepSecondBand;
	using obvr::camera::kSweepThirdBand;
	using obvr::camera::SweepProbeStage;

	// Anything but the sweep value is the setting the person asked for, and
	// the world render number does not enter into it. This is the flow every
	// ordinary run takes.
	for (UInt32 configured = 0; configured < 3; ++configured) {
		Check(SweepProbeStage(0, configured) == configured,
		      "a configured rung is used as configured");
		Check(SweepProbeStage(1000, configured) == configured,
		      "and stays that rung however long the run goes on");
	}

	// The sweep itself, band by band, each checked at its first and last
	// world render so a band that slipped by one is a failure rather than a
	// surprise in the log.
	//
	// The opening band is the one the first sweep was missing: it has to
	// start at the very first world render, because by render 260 the HUD
	// had already stopped drawing and no later band could tell why.
	Check(SweepProbeStage(0, kProbeSweep) == kProbeSinglePass,
	      "the sweep opens with no dual pass at all, from the first frame");
	Check(SweepProbeStage(kSweepFirstBand - 1, kProbeSweep) == kProbeSinglePass,
	      "and stays there to the last render before the first band");

	Check(SweepProbeStage(kSweepFirstBand, kProbeSweep) == 1,
	      "the first band adds the second render, and nothing else");
	Check(SweepProbeStage(kSweepSecondBand - 1, kProbeSweep) == 1, "for the whole band");

	Check(SweepProbeStage(kSweepSecondBand, kProbeSweep) == kProbeSinglePass,
	      "the second band takes the second render away again");
	Check(SweepProbeStage(kSweepThirdBand - 1, kProbeSweep) == kProbeSinglePass,
	      "for the whole band");

	Check(SweepProbeStage(kSweepThirdBand, kProbeSweep) == 0,
	      "and the last band runs the whole mechanism");
	Check(SweepProbeStage(kSweepThirdBand + 10000, kProbeSweep) == 0,
	      "and leaves it there, however long the person stands still");

	// The property this sweep rests on, and the one the first sweep could
	// not test: the rung it opens with comes back partway through. If the
	// HUD draws in the first band and not in the third, the switch is one
	// way, and that is a different bug from one the rung keeps setting.
	Check(SweepProbeStage(0, kProbeSweep) == SweepProbeStage(kSweepSecondBand, kProbeSweep),
	      "the opening rung returns after the second render has been tried");

	// And every world render has exactly one rung, none of them outside the
	// four the probe knows.
	for (UInt32 call = 0; call <= kSweepThirdBand + 100; ++call) {
		const UInt32 stage = SweepProbeStage(call, kProbeSweep);
		if (stage > kProbeSinglePass) {
			Check(false, "the sweep named a rung the dual pass does not have");
			break;
		}
	}
	Check(true, "every world render maps to a rung the dual pass has");
}

void TestSecondPassUnderProbe() {
	std::printf("When the probe refuses the second pass\n");

	using obvr::camera::kProbeSinglePass;
	using obvr::camera::WantsSecondScenePass;

	// Rung 3 refuses, whatever the frame wanted - that is the whole point of
	// it, and the two flows that would otherwise have run twice.
	Check(!WantsSecondScenePass(true, true, false, false, kProbeSinglePass),
	      "the rung that cuts the second render cuts it on a frame that wanted it");
	Check(!WantsSecondScenePass(true, true, true, true, kProbeSinglePass),
	      "including a menu asked to hang in the world");

	// Every other rung leaves the plain decision exactly as it was, across all
	// sixteen of its input combinations. This is what stops the probe from
	// changing behaviour when nobody asked it to.
	for (UInt32 rung = 0; rung < 3; ++rung) {
		for (int bits = 0; bits < 16; ++bits) {
			const bool frameOpen = (bits & 1) != 0;
			const bool armed = (bits & 2) != 0;
			const bool menuIsUp = (bits & 4) != 0;
			const bool menusInWorld = (bits & 8) != 0;
			if (WantsSecondScenePass(frameOpen, armed, menuIsUp, menusInWorld, rung) !=
			    WantsSecondScenePass(frameOpen, armed, menuIsUp, menusInWorld)) {
				Check(false, "a probe rung changed a decision it has no business changing");
				return;
			}
		}
	}
	Check(true, "every other rung leaves the decision exactly as it was");

	// And the refusal does not resurrect a frame that had no second pass to
	// begin with: rung 3 refuses those too, rather than flipping them.
	for (int bits = 0; bits < 16; ++bits) {
		const bool frameOpen = (bits & 1) != 0;
		const bool armed = (bits & 2) != 0;
		const bool menuIsUp = (bits & 4) != 0;
		const bool menusInWorld = (bits & 8) != 0;
		if (WantsSecondScenePass(frameOpen, armed, menuIsUp, menusInWorld, kProbeSinglePass)) {
			Check(false, "the cutting rung let a second pass through");
			return;
		}
	}
	Check(true, "the cutting rung refuses every frame, however the frame was shaped");
}

void TestDeliversDualEyes() {
	std::printf("When the frame promises two captured eyes\n");

	using obvr::camera::DeliversDualEyes;
	using obvr::camera::kProbeSinglePass;

	// The one flow that promises them: dual stereo, a hooked scene render,
	// and a rung that lets the second render happen.
	Check(DeliversDualEyes(true, true, 0), "dual stereo on a hooked render delivers two eyes");

	// Each refusal on its own, so a change that collapses two of them into
	// one is a failure rather than a subtlety.
	Check(!DeliversDualEyes(false, true, 0), "another stereo mode captures nothing to submit");
	Check(!DeliversDualEyes(true, false, 0),
	      "an unhooked scene render never runs the passes that capture");
	Check(!DeliversDualEyes(true, true, kProbeSinglePass),
	      "the rung that cuts the second render leaves only one picture");

	// The property this exists for: whenever the second pass will not run,
	// the frame must not promise two eyes - or the submit waits on captures
	// that never arrive and the headset holds the last pair it got.
	for (int bits = 0; bits < 16; ++bits) {
		const bool frameOpen = (bits & 1) != 0;
		const bool armed = (bits & 2) != 0;
		const bool menuIsUp = (bits & 4) != 0;
		const bool menusInWorld = (bits & 8) != 0;
		if (obvr::camera::WantsSecondScenePass(frameOpen, armed, menuIsUp, menusInWorld,
		                                       kProbeSinglePass)) {
			Check(false, "the cutting rung ran a second pass after all");
			return;
		}
		if (DeliversDualEyes(true, true, kProbeSinglePass)) {
			Check(false, "a frame promised two eyes on a rung that renders once");
			return;
		}
	}
	Check(true, "a rung that renders once never promises two eyes");

	// And every other rung keeps the promise it always made, so switching
	// the probe off restores exactly the previous behaviour.
	for (UInt32 rung = 0; rung < 3; ++rung) {
		if (!DeliversDualEyes(true, true, rung)) {
			Check(false, "an ordinary rung stopped delivering two eyes");
			return;
		}
	}
	Check(true, "every ordinary rung delivers two eyes exactly as before");
}

void TestAimPitchWanted() {
	std::printf("Aim follows the gaze - when\n");

	using obvr::camera::AimPitchWanted;

	// (enabled, headset, thirdPerson, thirdPersonAllowed, menu, attacking)
	Check(AimPitchWanted(true, true, false, true, false, false),
	      "switched on, headset delivering, first person, no menu: the gaze aims");

	// First person needs no attack - the pitch is written on every frame
	// there, because that camera does not depend on it.
	Check(AimPitchWanted(true, true, false, false, false, false),
	      "first person aims whether or not anything is being aimed, and whatever the "
	      "third person switch says");

	Check(!AimPitchWanted(false, true, false, true, false, true), "switched off, nothing is written");

	// Without a headset the head decides nothing, so the mouse is still the
	// only way to aim. Writing a pitch then would take the player's aim away
	// on a machine that never asked for VR.
	Check(!AimPitchWanted(true, false, false, true, false, true),
	      "no headset: the mouse keeps the aim it has always had");

	// Third person, measured: the engine builds that camera from rotX, so the
	// field is written only while something is aimed, and only with the
	// switch on.
	Check(AimPitchWanted(true, true, true, true, false, true),
	      "third person, allowed, aiming: the pitch is borrowed for the shot");
	Check(!AimPitchWanted(true, true, true, true, false, false),
	      "third person, allowed, not aiming: the mouse keeps its tilt");
	Check(!AimPitchWanted(true, true, true, false, false, true),
	      "third person with the switch off is left alone even while aiming");

	// The trap the crosshair fell into first: with Menus=world a dialogue is a
	// menu over a world that is still being drawn, so "the world is live" is
	// not the same question as "nothing is open".
	Check(!AimPitchWanted(true, true, false, true, true, true),
	      "nothing is aimed while a menu is up");

	// Every combination, so that a gate added later without a test still gets
	// exercised here.
	for (int bits = 0; bits < 64; ++bits) {
		const bool enabled = (bits & 1) != 0;
		const bool headset = (bits & 2) != 0;
		const bool third = (bits & 4) != 0;
		const bool allowed = (bits & 8) != 0;
		const bool menu = (bits & 16) != 0;
		const bool attacking = (bits & 32) != 0;
		const bool expected =
			enabled && headset && !menu && (!third || (allowed && attacking));
		if (AimPitchWanted(enabled, headset, third, allowed, menu, attacking) != expected) {
			Check(false, "one of the sixty-four gate combinations disagrees");
			return;
		}
	}
	Check(true, "all sixty-four combinations of the six gates agree");
}

void TestPlayerPitchForGaze() {
	std::printf("Aim follows the gaze - how far\n");

	using obvr::camera::kAimPitchLimitRadians;
	using obvr::camera::PlayerPitchForGaze;

	const auto Near = [](float a, float b) { return a - b < 1e-3f && b - a < 1e-3f; };

	Check(Near(PlayerPitchForGaze(0.0f), 0.0f), "a level view aims level");

	// THE test of this whole piece of work. OBVR's pitch is positive looking
	// up; Oblivion's rotX is positive looking DOWN, from two sources found
	// separately. So the signs must come out opposite, and getting this
	// backwards aims at the floor when the wearer looks at the sky - which is
	// worse than not aiming at all, and is why the write waited for evidence.
	Check(PlayerPitchForGaze(0.5f) < 0.0f, "looking up gives Oblivion a negative pitch");
	Check(PlayerPitchForGaze(-0.5f) > 0.0f, "looking down gives Oblivion a positive pitch");

	// The magnitude, against a sine anybody can check: 0.5 is 30 degrees, and
	// 30 degrees is 0.5236 radians.
	Check(Near(PlayerPitchForGaze(0.5f), -0.5235988f), "a sine of 0.5 is 30 degrees up");
	Check(Near(PlayerPitchForGaze(-0.5f), 0.5235988f), "and 0.5 down is 30 the other way");

	// Level is level from both sides - a rounding error here would leave the
	// player permanently aiming a hair off horizontal.
	Check(Near(PlayerPitchForGaze(0.5f), -PlayerPitchForGaze(-0.5f)),
	      "up and down are mirror images of each other");

	// Straight up and straight down are outside what Oblivion's own input can
	// reach: the Construction Set wiki gives the true range as -89 to 89, and
	// the identity behind Asin divides by a cosine that is zero at the pole.
	Check(Near(PlayerPitchForGaze(1.0f), -kAimPitchLimitRadians),
	      "straight up is held at the engine's own 89 degrees");
	Check(Near(PlayerPitchForGaze(-1.0f), kAimPitchLimitRadians),
	      "and straight down at 89 the other way");

	// Past the pole is not a view anybody has, but a NaN or a garbage float
	// reaching here must still come out as an angle rather than as an
	// infinity written into the player.
	Check(Near(PlayerPitchForGaze(4.0f), -kAimPitchLimitRadians),
	      "a sine past 1 is clamped rather than turned into nonsense");
	Check(Near(PlayerPitchForGaze(-4.0f), kAimPitchLimitRadians),
	      "and so is one past -1");

	// Monotonic across the range: every step further up must aim further up.
	// A clamp written as a comparison against the wrong bound would pass every
	// check above and still flatten one half of the sweep.
	float previous = PlayerPitchForGaze(-0.95f);
	for (int step = -18; step <= 19; ++step) {
		const float sine = static_cast<float>(step) * 0.05f;
		const float pitch = PlayerPitchForGaze(sine);
		if (pitch > previous) {
			Check(false, "the pitch stops falling as the view rises");
			return;
		}
		previous = pitch;
	}
	Check(true, "the pitch falls steadily as the view rises, across the whole range");
}

void TestAtan2() {
	std::printf("Angles from components\n");

	using obvr::math::Atan2;
	using obvr::math::kHalfPi;
	using obvr::math::kPi;

	const auto Near = [](float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; };

	// The four axes, where the ratio the single-argument atan works on is
	// either zero or undefined.
	Check(Near(Atan2(0.0f, 1.0f), 0.0f), "straight along +x is zero");
	Check(Near(Atan2(1.0f, 0.0f), kHalfPi), "straight along +y is a quarter turn");
	Check(Near(Atan2(0.0f, -1.0f), kPi), "straight along -x is half a turn");
	Check(Near(Atan2(-1.0f, 0.0f), -kHalfPi), "straight along -y is a quarter turn back");

	// The four quadrants. This is the whole reason atan alone will not do: the
	// ratio y/x cannot tell a direction from its opposite, so the second and
	// third quadrants would come back as the fourth and first.
	Check(Near(Atan2(1.0f, 1.0f), kPi / 4.0f), "up and right is an eighth turn");
	Check(Near(Atan2(1.0f, -1.0f), 3.0f * kPi / 4.0f), "up and left is three eighths");
	Check(Near(Atan2(-1.0f, -1.0f), -3.0f * kPi / 4.0f), "down and left is three eighths back");
	Check(Near(Atan2(-1.0f, 1.0f), -kPi / 4.0f), "down and right is an eighth back");

	// No direction at all. Any answer here is arbitrary, so the one that
	// cannot turn a character is the one to give.
	Check(Near(Atan2(0.0f, 0.0f), 0.0f), "no direction reads as no turn");
}

void TestWrapAngle() {
	std::printf("Angles brought back into range\n");

	using obvr::math::kPi;
	using obvr::math::WrapAngle;

	const auto Near = [](float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; };

	Check(Near(WrapAngle(0.0f), 0.0f), "zero stays zero");
	Check(Near(WrapAngle(1.0f), 1.0f), "an angle already in range is untouched");

	// The point of it: a heading a hair past half a turn is a hair short of
	// half a turn the other way, not most of a turn away.
	Check(Near(WrapAngle(kPi + 0.1f), -kPi + 0.1f), "just past half a turn comes back the short way");
	Check(Near(WrapAngle(-kPi - 0.1f), kPi - 0.1f), "and just past it the other way does too");

	Check(WrapAngle(10.0f) <= kPi && WrapAngle(10.0f) >= -kPi, "a large angle lands in range");
	Check(WrapAngle(-10.0f) <= kPi && WrapAngle(-10.0f) >= -kPi, "so does a large negative one");
}

void TestAimYaw() {
	std::printf("The body turned to face the gaze\n");

	using obvr::camera::AimYawWanted;
	using obvr::camera::PlayerYawForGaze;
	using obvr::math::kTwoPi;

	const auto Near = [](float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; };

	// The gate is everything the pitch asks, plus the attack control. That
	// last one is the difference between the two halves: the pitch follows the
	// head always, the body only while something is being aimed, and stands
	// still for ordinary looking around.
	// (enabled, headset, thirdPerson, thirdPersonAllowed, menu, attacking)
	Check(AimYawWanted(true, true, false, true, false, true),
	      "aiming with the attack held turns the body");
	Check(!AimYawWanted(true, true, false, true, false, false),
	      "merely looking around does not turn the body");
	Check(!AimYawWanted(false, true, false, true, false, true), "switched off, nothing turns");
	Check(!AimYawWanted(true, false, false, true, false, true), "no headset, nothing turns");
	Check(AimYawWanted(true, true, true, true, false, true),
	      "third person turns the body too, on its own switch");
	Check(!AimYawWanted(true, true, true, false, false, true),
	      "and with that switch off third person is left alone");
	Check(!AimYawWanted(true, true, true, true, false, false),
	      "third person still needs something to be aimed");
	Check(AimYawWanted(true, true, false, false, false, true),
	      "the third person switch says nothing about first person");
	Check(!AimYawWanted(true, true, false, true, true, true), "and nothing turns while a menu is up");

	// A head that is not turned leaves the heading exactly as the engine had
	// it. Anything else would nudge the character every frame it aimed.
	Check(Near(PlayerYawForGaze(1.0f, 0.0f), 1.0f), "a head facing forward changes nothing");

	// The turn is subtracted, because Oblivion's heading grows clockwise while
	// the matrix column OBVR reads runs the other way.
	Check(Near(PlayerYawForGaze(1.0f, 0.5f), 0.5f), "a head turned one way takes the body with it");
	Check(Near(PlayerYawForGaze(1.0f, -0.5f), 1.5f), "and turned the other way, the other way");

	// Across the seam at zero, in both directions. Oblivion reports headings
	// as 0..360, and a character told to face -0.4 radians is a character
	// facing an angle the engine never produces.
	const float belowZero = PlayerYawForGaze(0.1f, 0.5f);
	Check(belowZero > 0.0f && belowZero < kTwoPi, "a turn past north stays inside a full circle");
	Check(Near(belowZero, 0.1f - 0.5f + kTwoPi), "and comes out on the far side of it");

	const float aboveTwoPi = PlayerYawForGaze(6.2f, -0.5f);
	Check(aboveTwoPi >= 0.0f && aboveTwoPi < kTwoPi, "and a turn the other way past north as well");
	Check(Near(aboveTwoPi, 6.2f + 0.5f - kTwoPi), "landing just past zero");

	// Every combination stays in range, which is the invariant the engine
	// cares about. Half a circle each way is more than a head can turn.
	for (int base = 0; base < 64; ++base) {
		for (int turn = -16; turn <= 16; ++turn) {
			const float engineYaw = static_cast<float>(base) * (kTwoPi / 64.0f);
			const float headYaw = static_cast<float>(turn) * 0.1f;
			const float result = PlayerYawForGaze(engineYaw, headYaw);
			if (!(result >= 0.0f && result < kTwoPi)) {
				Check(false, "a heading escaped the circle");
				return;
			}
		}
	}
	Check(true, "every heading and turn together stays inside one circle");
}

void TestReleaseClock() {
	std::printf("The clock that says a shot is in flight\n");

	using obvr::camera::kReleaseClockIdle;
	using obvr::camera::NextReleaseClock;
	using obvr::camera::ReleaseClockInput;

	const float frame = 1.0f / 60.0f;

	const auto Step = [&](float current, bool held, bool wasHeld, bool castTurning,
	                      bool castWasOpen, bool castFeeds) {
		ReleaseClockInput input;
		input.attackHeld = held;
		input.attackWasHeld = wasHeld;
		input.castTurning = castTurning;
		input.castWasOpen = castWasOpen;
		input.castFeedsClock = castFeeds;
		input.deltaSeconds = frame;
		return NextReleaseClock(current, input);
	};

	// Held stops it, whichever control is holding.
	Check(Step(0.5f, true, true, false, false, true) == kReleaseClockIdle,
	      "holding the attack control stops the clock");
	Check(Step(0.5f, false, false, true, true, true) == kReleaseClockIdle,
	      "and so does the cast window while it is turning the body");

	// Letting go starts it.
	Check(Step(kReleaseClockIdle, false, true, false, false, true) == 0.0f,
	      "releasing the attack control starts it at zero");

	// THE FLOW THAT WAS WRONG. A cast may start the clock only while the window
	// is what turns the body. Once the hook does the turning it must not,
	// because a started clock is what lets the bow's machinery recognise a shot
	// - and a cast reads to the engine as an attack, so it would hold the body
	// turned for the whole animation.
	Check(Step(kReleaseClockIdle, false, false, false, true, true) == 0.0f,
	      "a cast window starts the clock while it owns the turn");
	Check(Step(kReleaseClockIdle, false, false, false, true, false) == kReleaseClockIdle,
	      "and starts nothing once the hook owns it - the fault that made the hook look "
	      "like it had changed nothing");

	// Counting once started, from either source.
	const float counting = Step(0.0f, false, false, false, false, false);
	Check(counting > frame * 0.9f && counting < frame * 1.1f,
	      "a started clock counts up by the frame time");

	// The hook starts it directly, and nothing must take it away again on the
	// next frame - that is the one moment a return is owed.
	Check(Step(0.0f, false, false, false, true, false) > 0.0f,
	      "a clock the hook started keeps counting even with the cast window still open");

	// Idle stays idle. The ordinary frame, which is nearly every frame.
	Check(Step(kReleaseClockIdle, false, false, false, false, true) == kReleaseClockIdle,
	      "with nothing happening it stays stopped");
}

void TestCastWindow() {
	std::printf("How long the body stays turned for a spell\n");

	using obvr::camera::CastWindow;
	using obvr::camera::CastWindowInput;
	using obvr::camera::kCastWindowLimitSeconds;
	using obvr::camera::NextCastWindow;

	const float minimum = 0.25f;
	const float frame = 1.0f / 60.0f;

	// leaving: the action field reading Attack rather than AttackFollowThrough,
	// which is the same signal the bow's window closes on.
	const auto Step = [&](CastWindow window, bool held, bool wasHeld, bool leaving) {
		CastWindowInput input;
		input.enabled = true;
		input.castHeld = held;
		input.castWasHeld = wasHeld;
		input.spellStillLeaving = leaving;
		input.deltaSeconds = frame;
		return NextCastWindow(window, input, minimum);
	};

	// Switched off closes it whatever else is true, including a press. The
	// setting has to be able to take the feature away completely.
	CastWindowInput off;
	off.enabled = false;
	off.castHeld = true;
	off.deltaSeconds = frame;
	Check(!NextCastWindow(CastWindow{true, 0.1f}, off, minimum).open,
	      "switched off, the window is shut even mid-cast");

	// Nothing happening. The ordinary frame, which is nearly all of them.
	Check(!Step(CastWindow{}, false, false, false).open,
	      "with no press there is no window");
	Check(!Step(CastWindow{}, true, true, false).open,
	      "and a key already down when this starts does not open one - only the edge does");

	// The press opens it.
	const CastWindow opened = Step(CastWindow{}, true, false, false);
	Check(opened.open && opened.secondsOpen == 0.0f, "the press opens the window at zero");

	// THE ACTION FIELD SAYING NOTHING. A cast that the engine never reports as
	// an attack at all - which is what was expected before the trace showed
	// otherwise, and is still what a spell type nobody has cast yet might do.
	CastWindow blind = opened;
	int frames = 0;
	while (blind.open && frames < 600) {
		blind = Step(blind, false, false, false);
		++frames;
	}
	const float blindHeld = static_cast<float>(frames) * frame;
	Check(blindHeld > minimum - frame && blindHeld < minimum + frame * 2.0f,
	      "with the action field silent the window lasts its fixed minimum and then closes");

	// AN ATTACK THAT NEVER FINISHES. It must not be able to hold the body turned
	// for good, because that is the wearer walking sideways for the rest of the
	// session.
	CastWindow stuck = opened;
	frames = 0;
	while (stuck.open && frames < 6000) {
		stuck = Step(stuck, false, false, true);
		++frames;
	}
	const float stuckHeld = static_cast<float>(frames) * frame;
	// Within a frame or two of the limit, not to the millisecond: the window
	// adds a frame time at a go and a sum of ninety of those does not land on
	// the same float as one multiplication. What is being claimed is that the
	// window is BOUNDED, and a tighter check would only be testing arithmetic
	// rounding.
	Check(stuck.open == false && stuckHeld <= kCastWindowLimitSeconds + frame * 3.0f,
	      "an attack that never finishes cannot hold the body past the limit");

	// THE MEASURED CASE. Held open through the Attack frames, closed the frame
	// the field reads FollowThrough - which is 54 frames in, and is the whole
	// reason this stopped using a casting flag that could not tell the two
	// apart.
	CastWindow tracking = opened;
	for (int i = 0; i < 30; ++i) {
		tracking = Step(tracking, false, false, true);
	}
	Check(tracking.open && tracking.secondsOpen > minimum,
	      "Attack holds the window open past the minimum");
	tracking = Step(tracking, false, false, false);
	Check(!tracking.open, "and FollowThrough closes it, because the spell has gone by then");

	// The key held keeps it open on its own, for a binding that is held rather
	// than tapped - a gamepad trigger, or someone leaning on the key.
	CastWindow leaning = opened;
	for (int i = 0; i < 30; ++i) {
		leaning = Step(leaning, true, true, false);
	}
	Check(leaning.open, "holding the cast key keeps the window open with no action at all");

	// A second press while the window stands restarts it rather than being
	// swallowed, so casting twice quickly turns for both.
	CastWindow again = Step(CastWindow{true, 1.0f}, true, false, false);
	Check(again.open && again.secondsOpen == 0.0f, "a fresh press restarts the window");

	// A minimum of zero is a real setting - what AimCastHoldSeconds becomes if
	// the flag proves to track the cast - and must not open a window that never
	// closes or one that closes before it opens.
	CastWindowInput noMinimum;
	noMinimum.enabled = true;
	noMinimum.deltaSeconds = frame;
	CastWindow bare = NextCastWindow(CastWindow{}, [&] {
		CastWindowInput press = noMinimum;
		press.castHeld = true;
		return press;
	}(), 0.0f);
	Check(bare.open, "with no minimum the press still opens the window");
	bare = NextCastWindow(bare, noMinimum, 0.0f);
	Check(!bare.open, "and with nothing holding it, it closes at once");
}

void TestAimArcCorrection() {
	std::printf("Putting the eye back on the arc the body's turn walked it along\n");

	using obvr::camera::AimArcCorrection;
	using obvr::camera::AimArcInput;
	using obvr::NiPoint3;
	using obvr::math::kPi;

	const auto Near = [](float a, float b, float tolerance = 1e-4f) {
		return a - b < tolerance && b - a < tolerance;
	};

	// An arm of 4 units along +x from a centre at the origin, which is the
	// shape the game gives: the eye standing a few units off the axis the body
	// turns about.
	const auto Arm = [](float x, float y, float offset) {
		AimArcInput input;
		input.cameraPosition = NiPoint3{x, y, 100.0f};
		input.turnCentre = NiPoint3{0.0f, 0.0f, 0.0f};
		input.centreKnown = true;
		input.bodyOffset = offset;
		return AimArcCorrection(input);
	};

	// Nothing to undo. Both refusals matter: the offset is zero for all but a
	// few frames of a shot, and the centre is unknown whenever the player
	// cannot be reached.
	Check(Near(Arm(4.0f, 0.0f, 0.0f).x, 0.0f) && Near(Arm(4.0f, 0.0f, 0.0f).y, 0.0f),
	      "a body holding no turn moves the eye not at all");

	AimArcInput noCentre;
	noCentre.cameraPosition = NiPoint3{4.0f, 0.0f, 100.0f};
	noCentre.centreKnown = false;
	noCentre.bodyOffset = 0.5f;
	Check(Near(AimArcCorrection(noCentre).x, 0.0f) && Near(AimArcCorrection(noCentre).y, 0.0f),
	      "and an unreadable centre leaves the camera alone rather than guessing one");

	// The eye standing exactly on the axis is not swung at all, so there is
	// nothing to put back however far the body turns.
	Check(Near(Arm(0.0f, 0.0f, 1.0f).x, 0.0f) && Near(Arm(0.0f, 0.0f, 1.0f).y, 0.0f),
	      "an eye on the axis needs no correction");

	// A quarter turn, where the answer can be written down. The correction
	// turns the arm BACKWARDS by the offset, so an arm along +x comes back to
	// -y, and the move is the difference between the two.
	const NiPoint3 quarter = Arm(4.0f, 0.0f, kPi * 0.5f);
	Check(Near(quarter.x, -4.0f) && Near(quarter.y, -4.0f),
	      "a quarter turn moves the eye from +x to -y");

	// The other way round, which is the same size in the other direction.
	const NiPoint3 back = Arm(4.0f, 0.0f, -kPi * 0.5f);
	Check(Near(back.x, -4.0f) && Near(back.y, 4.0f), "and turning the other way mirrors it");

	// The correction is a ROTATION about the centre, so wherever it puts the
	// eye, it puts it the same distance out. This is the property that makes it
	// safe: it can never push the camera into or out of the world.
	for (int step = -6; step <= 6; ++step) {
		const float offset = static_cast<float>(step) * 0.4f;
		const NiPoint3 move = Arm(3.0f, 4.0f, offset);
		const float x = 3.0f + move.x;
		const float y = 4.0f + move.y;
		if (!Near(x * x + y * y, 25.0f, 0.01f)) {
			Check(false, "the corrected eye stays the same distance from the centre");
			return;
		}
	}
	Check(true, "the corrected eye stays the same distance from the centre, at every angle");

	// The turn is about the vertical, so it cannot change how high the eye
	// stands - and z belongs to the head tracking, which would fight anything
	// written here.
	Check(Near(Arm(4.0f, 0.0f, 1.0f).z, 0.0f), "and it never touches the height");

	// A centre away from the origin, because the game's is: the arm is the
	// difference, not the camera's own position.
	AimArcInput offCentre;
	offCentre.cameraPosition = NiPoint3{2044.0f, 4676.0f, 176.0f};
	offCentre.turnCentre = NiPoint3{2040.0f, 4676.0f, 57.0f};
	offCentre.centreKnown = true;
	offCentre.bodyOffset = kPi * 0.5f;
	const NiPoint3 shifted = AimArcCorrection(offCentre);
	Check(Near(shifted.x, -4.0f) && Near(shifted.y, -4.0f),
	      "the arm is measured from the centre, not from the world origin");

	// The size of the step this exists to cancel, against the numbers the
	// headset actually produced: an arm of 4.58 units and a give-back of 33
	// degrees moved the eye 2.6 units, and the correction has to be that same
	// 2.6 the other way.
	const float armLength = 4.58f;
	const float turn = 33.0f * kPi / 180.0f;
	const NiPoint3 measured = Arm(armLength, 0.0f, turn);
	const float moved =
		obvr::math::Sqrt(measured.x * measured.x + measured.y * measured.y);
	Check(Near(moved, 2.60f, 0.02f), "and it is the size the log measured: 2.6 units at 33 degrees");
}

void TestAimYawRemaining() {
	std::printf("How much turn the body still owes the gaze\n");

	using obvr::camera::AimYawRemaining;
	using obvr::math::kPi;
	using obvr::math::kTwoPi;

	const auto Near = [](float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; };

	// Nothing handed over yet, so the whole of the head's turn is still owed.
	Check(Near(AimYawRemaining(0.5f, 0.0f), 0.5f), "with nothing given, all of the turn remains");
	Check(Near(AimYawRemaining(-0.5f, 0.0f), -0.5f), "and the same turning the other way");

	// Arrived. This is the property the whole design rests on: with the head
	// held still the remainder falls to zero and the body stops, rather than
	// turning for as long as the head is off centre.
	Check(Near(AimYawRemaining(0.5f, 0.5f), 0.0f), "a body already round owes nothing");
	Check(Near(AimYawRemaining(-1.2f, -1.2f), 0.0f), "and the same the other way");

	// Part way.
	Check(Near(AimYawRemaining(0.8f, 0.3f), 0.5f), "half handed over leaves the rest");
	Check(Near(AimYawRemaining(0.3f, 0.8f), -0.5f), "and overshooting owes a turn back");

	// The head came back to centre while the body stayed turned, which is what
	// letting go of the attack control and looking away leaves behind. The
	// remainder is the turn back, not a full circle of it.
	Check(Near(AimYawRemaining(0.0f, 1.0f), -1.0f), "a centred head owes the offset back");

	// Across the seam. An offset just under a full circle and a head just over
	// zero are next to each other, and a subtraction without a wrap would call
	// that nearly a whole turn.
	Check(Near(AimYawRemaining(0.1f, kTwoPi - 0.1f), 0.2f),
	      "either side of north is a short way apart");
	Check(Near(AimYawRemaining(kTwoPi - 0.1f, 0.1f), -0.2f), "and the same the other way round");

	// Never more than half a circle, whatever goes in - which is what keeps a
	// step from sending the body the long way round.
	for (int h = -20; h <= 20; ++h) {
		for (int o = -20; o <= 20; ++o) {
			const float remaining =
				AimYawRemaining(static_cast<float>(h) * 0.4f, static_cast<float>(o) * 0.4f);
			if (!(remaining >= -kPi - 0.001f && remaining <= kPi + 0.001f)) {
				Check(false, "a remainder escaped the short way round");
				return;
			}
		}
	}
	Check(true, "every head and offset together stays on the short way round");
}

void TestYawWriteLanded() {
	std::printf("Whether a written heading reached the player\n");

	using obvr::camera::kYawLandingMinStep;
	using obvr::camera::YawWriteLanded;
	using obvr::math::kTwoPi;

	// Too small a step to tell anything by, and the measured behaviour - that
	// a written heading does survive - stands. Answering "did not land" here
	// would take a real turn back out of the camera on the strength of the
	// mouse's own movement.
	Check(YawWriteLanded(1.0f, 1.4f, 0.0f), "no step at all counts as landed");
	Check(YawWriteLanded(1.0f, 1.4f, kYawLandingMinStep * 0.5f),
	      "and a step too small to measure does too");

	// Found where it was left: the write landed, which is the ordinary case.
	Check(YawWriteLanded(1.0f, 1.0f, 0.5f), "a heading found where it was left has landed");
	Check(YawWriteLanded(1.0f, 1.02f, 0.5f), "and one the mouse nudged slightly still has");

	// Moved back by about the step: something else set the player's heading,
	// so the body never took the turn the offset is claiming.
	Check(!YawWriteLanded(1.0f, 1.5f, 0.5f), "a heading put back where it was has not landed");
	Check(!YawWriteLanded(1.0f, 0.5f, -0.5f), "and the same for a step the other way");

	// Across north, which is where a comparison without a wrap goes wrong: a
	// heading a hair above zero and one a hair below it are next to each
	// other, not a whole circle apart.
	Check(YawWriteLanded(0.05f, kTwoPi - 0.01f, 0.5f), "either side of north is the same heading");
	Check(YawWriteLanded(kTwoPi - 0.05f, 0.02f, 0.5f), "and the same the other way round");

	// The dividing line sits at half the step, so a step of any size splits the
	// two outcomes with the same room on each side.
	Check(YawWriteLanded(1.0f, 1.0f, 2.0f), "a large step judges the same way");
	Check(!YawWriteLanded(1.0f, 3.0f, 2.0f), "and its opposite outcome too");
}

void TestAimYawHoldsTheView() {
	std::printf("The view holds still while the body turns\n");

	using obvr::camera::AimYawRemaining;
	using obvr::camera::PlayerYawForGaze;
	using obvr::math::WrapAngle;

	const auto Near = [](float a, float b, float tolerance = 1e-4f) {
		return a - b < tolerance && b - a < tolerance;
	};

	// The invariant the sideways aim is built on, run end to end over the pure
	// functions. The camera's heading is the negative of the player's - that is
	// the measurement the aim probe made - with the head's turn added on top
	// and the offset taken back out again at the base:
	//
	//     view = -rotZ - offset + headYaw
	//
	// Turning the body means rotZ falls by the step while the offset rises by
	// it, and those two cancel in that expression. So the wearer keeps looking
	// at exactly what they were looking at, however far the body comes round.
	// If they did not cancel, this is the test that would say so.
	const auto view = [](float rotZ, float offset, float headYaw) {
		return WrapAngle(-rotZ - offset + headYaw);
	};

	float rotZ = 2.0f;
	float offset = 0.0f;
	const float headYaw = 0.9f;
	const float before = view(rotZ, offset, headYaw);

	// One frame at full speed, which is the default: the whole remainder in a
	// single step.
	const float step = AimYawRemaining(headYaw, offset);
	rotZ = PlayerYawForGaze(rotZ, step);
	offset = WrapAngle(offset + step);

	Check(Near(view(rotZ, offset, headYaw), before), "the view is where it was before the turn");
	Check(Near(AimYawRemaining(headYaw, offset), 0.0f), "and the body faces the gaze");

	// Eased instead, over many frames. Each step is a share of what is left,
	// and the view has to hold still on every one of them - a correction that
	// only balanced at the end would show as the body creeping the view round.
	rotZ = 2.0f;
	offset = 0.0f;
	for (int frame = 0; frame < 40; ++frame) {
		const float eased = AimYawRemaining(headYaw, offset) * 0.25f;
		rotZ = PlayerYawForGaze(rotZ, eased);
		offset = WrapAngle(offset + eased);
		if (!Near(view(rotZ, offset, headYaw), before)) {
			Check(false, "the view moved during an eased turn");
			return;
		}
	}
	Check(true, "the view holds still on every frame of an eased turn");
	Check(Near(AimYawRemaining(headYaw, offset), 0.0f, 0.01f), "and the body arrives");

	// The head turns on while the body is following, which is the real case -
	// nobody holds their head still while drawing a bow.
	rotZ = 2.0f;
	offset = 0.0f;
	for (int frame = 0; frame < 30; ++frame) {
		const float movingHead = 0.05f * static_cast<float>(frame);
		const float expected = view(rotZ, offset, movingHead);
		const float moved = AimYawRemaining(movingHead, offset);
		rotZ = PlayerYawForGaze(rotZ, moved);
		offset = WrapAngle(offset + moved);
		if (!Near(view(rotZ, offset, movingHead), expected)) {
			Check(false, "the view moved while the head was turning");
			return;
		}
	}
	Check(true, "a head turning while the body follows still leaves the view alone");

	// A step that did not land, taken back out again. The offset has to end up
	// where it started, or the base would be corrected for a turn the body
	// never made and the view would sit crooked by that much.
	offset = 0.3f;
	const float lost = AimYawRemaining(headYaw, offset);
	const float claimed = WrapAngle(offset + lost);
	Check(Near(WrapAngle(claimed - lost), 0.3f), "a step taken back leaves the offset as it was");
}


// When a spell's heading is turned - which is not when the spell is cast.
//
// This is the correction to a fault the headset found twice. The cast hook
// turned the heading inside MagicCaster::CastMagicItem, and spells still went
// out forwards. The log said why: the hook fires on a frame whose action field
// still reads None, and the animation that follows runs 53 frames of Attack
// before the field turns to AttackFollowThrough. The turn was nine tenths of a
// second early and long since given back by the time the spell left.
//
// The lead time therefore has to put the turn shortly BEFORE the end of the
// animation, not at its start and not at the change - OBVR's own reading of
// the bow records that on the frame the field first reads AttackFollowThrough
// the projectile has already gone.
void TestCastArm() {
	using obvr::camera::CastArm;
	using obvr::camera::CastArmDecision;
	using obvr::camera::CastArmInput;
	using obvr::camera::NextCastArm;

	std::printf("Arming a spell's turn\n");

	const float frame = 1.0f / 60.0f;
	CastArmInput input;
	input.deltaSeconds = frame;
	input.turnAfterSeconds = 0.70f;
	input.limitSeconds = 3.0f;
	input.actionIsAttack = true;

	// Nothing is watched until a cast says so.
	{
		CastArm arm;
		const CastArmDecision decision = NextCastArm(arm, input);
		Check(decision.next.seconds < 0.0f, "an idle arm stays idle");
		Check(!decision.turnNow, "and turns nothing");
		Check(!decision.missed, "and reports nothing");
		Check(decision.measuredSeconds == 0.0f, "and measures nothing");
	}

	// The cast arms it, and does not turn anything on the way in - which is the
	// whole correction to the version that turned inside the hook.
	{
		CastArmInput began = input;
		began.castBegan = true;
		began.actionIsAttack = false;  // the field still reads None here
		const CastArmDecision decision = NextCastArm(CastArm{}, began);
		Check(decision.next.seconds == 0.0f, "a cast starts the clock at zero");
		Check(!decision.turnNow, "and nothing is turned at the cast itself");
	}

	// The frames before the animation starts are not its end. The field reads
	// None for one or two of them, and mistaking that for the end would measure
	// a cast at nearly nothing and turn every spell after it far too early.
	{
		CastArmInput notYet = input;
		notYet.actionIsAttack = false;
		CastArm arm;
		arm.seconds = 0.0f;
		const CastArmDecision decision = NextCastArm(arm, notYet);
		Check(decision.measuredSeconds == 0.0f, "None before any Attack is not an ending");
		Check(!decision.missed, "and is not a miss");
		Check(decision.next.seconds > 0.0f, "the clock keeps running");
	}

	// A whole cast: 53 frames of Attack, then the field changes. The turn goes
	// in at the lead time and the duration comes back on the closing frame.
	{
		CastArm arm;
		arm.seconds = 0.0f;
		CastArmInput running = input;
		running.turnAfterSeconds = 0.70f;
		int turns = 0;
		float turnedAt = 0.0f;
		float measured = 0.0f;
		bool missed = false;
		for (int i = 0; i < 60; ++i) {
			running.actionIsAttack = i < 53;
			const CastArmDecision decision = NextCastArm(arm, running);
			if (decision.turnNow) {
				++turns;
				turnedAt = arm.seconds + frame;
			}
			if (decision.measuredSeconds > 0.0f) {
				measured = decision.measuredSeconds;
			}
			missed = missed || decision.missed;
			arm = decision.next;
		}
		// 53 frames at 60 Hz is 0.88 s, so a 0.70 s lead time fits inside it.
		Check(turns == 1, "the turn is made exactly once");
		Check(turnedAt >= 0.70f && turnedAt < 0.70f + frame * 2.0f, "at the lead time");
		Check(!missed, "and nothing is reported as missed");
		Check(measured > 0.88f && measured < 0.92f,
		      "the animation is measured at its real length");
	}

	// THE FAULT THIS EXISTS FOR. The same 53 frames at 90 Hz last 0.59 s, and a
	// lead time of 0.70 s never comes up: the spell leaves unaimed. It has to
	// be reported, and the duration has to come back so the next cast is right.
	{
		const float fastFrame = 1.0f / 90.0f;
		CastArm arm;
		arm.seconds = 0.0f;
		CastArmInput fast = input;
		fast.deltaSeconds = fastFrame;
		fast.turnAfterSeconds = 0.70f;
		int turns = 0;
		float measured = 0.0f;
		bool missed = false;
		for (int i = 0; i < 60; ++i) {
			fast.actionIsAttack = i < 53;
			const CastArmDecision decision = NextCastArm(arm, fast);
			if (decision.turnNow) {
				++turns;
			}
			if (decision.measuredSeconds > 0.0f) {
				measured = decision.measuredSeconds;
			}
			missed = missed || decision.missed;
			arm = decision.next;
		}
		Check(turns == 0, "a lead time longer than the animation turns nothing");
		Check(missed, "and says the spell left unaimed");
		Check(measured > 0.58f && measured < 0.62f,
		      "while still measuring how long the animation really was");

		// And the next cast, armed from that measurement, does turn.
		const float lead = measured - 0.12f;
		CastArm second;
		second.seconds = 0.0f;
		CastArmInput next = fast;
		next.turnAfterSeconds = lead;
		int secondTurns = 0;
		float secondTurnedAt = 0.0f;
		for (int i = 0; i < 60; ++i) {
			next.actionIsAttack = i < 53;
			const CastArmDecision decision = NextCastArm(second, next);
			if (decision.turnNow) {
				++secondTurns;
				secondTurnedAt = second.seconds + fastFrame;
			}
			second = decision.next;
		}
		Check(secondTurns == 1, "the cast after a measurement turns exactly once");
		Check(secondTurnedAt < 0.59f, "before the animation ends");
		Check(secondTurnedAt > 0.59f - 0.12f - fastFrame * 2.0f,
		      "and only a margin before it, so the body is barely turned at all");
	}

	// A cast that never reports an end must not leave this armed for good.
	{
		CastArmInput never = input;
		never.actionIsAttack = true;
		never.turnAfterSeconds = 100.0f;  // never reached before the limit
		CastArm arm;
		arm.seconds = 2.99f;
		arm.sawAttack = true;
		const CastArmDecision decision = NextCastArm(arm, never);
		Check(decision.next.seconds < 0.0f, "the limit disarms it");
		Check(decision.missed, "and says so");
		Check(!decision.turnNow, "without turning anything");
	}

	// The turn is made once, not once per frame for the rest of the animation.
	{
		CastArm arm;
		arm.seconds = 0.70f;
		arm.sawAttack = true;
		arm.turned = true;
		const CastArmDecision decision = NextCastArm(arm, input);
		Check(!decision.turnNow, "a turn already made is not made again");
	}

	// A second cast while one is armed simply becomes the one being watched.
	{
		CastArmInput again = input;
		again.castBegan = true;
		CastArm arm;
		arm.seconds = 0.5f;
		arm.sawAttack = true;
		arm.turned = true;
		const CastArmDecision decision = NextCastArm(arm, again);
		Check(decision.next.seconds == 0.0f, "the clock restarts from the newer cast");
		Check(!decision.next.turned, "with nothing carried over from the older one");
		Check(!decision.next.sawAttack, "and nothing remembered about its animation");
	}

	// A lead time of zero turns on the first Attack frame, which is what a
	// measurement shorter than the margin comes to.
	{
		CastArmInput immediate = input;
		immediate.turnAfterSeconds = 0.0f;
		CastArm arm;
		arm.seconds = 0.0f;
		const CastArmDecision decision = NextCastArm(arm, immediate);
		Check(decision.turnNow, "a zero lead time turns at once");
	}
}

void TestChaseCamera() {
	std::printf("The third person camera's share of the aim\n");

	using obvr::camera::AimCameraShare;
	using obvr::camera::ChaseStep;
	using obvr::camera::kChaseDeltaMult;

	const auto Near = [](float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; };

	// One step closes five percent of what is left, which is the measured
	// easing and the engine's own fChaseDeltaMult.
	Check(Near(ChaseStep(0.0f, 1.0f, kChaseDeltaMult), 0.05f), "the first frame takes five percent");
	Check(Near(ChaseStep(0.5f, 1.0f, kChaseDeltaMult), 0.525f),
	      "and every frame five percent of the remainder");
	Check(Near(ChaseStep(1.0f, 1.0f, kChaseDeltaMult), 1.0f), "arrived, nothing moves");
	Check(Near(ChaseStep(0.0f, 0.0f, kChaseDeltaMult), 0.0f), "and no turn at all stays at zero");

	// The curve the probe measured: four frames of easing leave 0.815 of the
	// remainder, and the log read 0.81 to 0.83 at every point of the sweep.
	// Written out as the sweep itself rather than a closed form, because the
	// closed form is what is being checked.
	float chased = 0.0f;
	for (int frame = 0; frame < 4; ++frame) {
		chased = ChaseStep(chased, 1.0f, kChaseDeltaMult);
	}
	Check(Near(1.0f - chased, 0.81450625f), "four frames leave 0.815 of the way to go, as measured");

	// Backwards as well, which is the return: a turn given back eases out the
	// same way it eased in.
	Check(Near(ChaseStep(1.0f, 0.0f, kChaseDeltaMult), 0.95f), "easing back is the same easing");

	// The short way round the circle. A target across the seam is a small
	// step, not a spin through the long arc.
	const float kPi = 3.14159265f;
	Check(ChaseStep(kPi - 0.1f, -kPi + 0.1f, kChaseDeltaMult) > kPi - 0.1f ||
	          ChaseStep(kPi - 0.1f, -kPi + 0.1f, kChaseDeltaMult) < -kPi + 0.2f,
	      "a target across the seam is approached by the short arc");

	// Which value the picture is corrected by, in each view.
	Check(Near(AimCameraShare(false, 0.4f, 0.1f), 0.4f),
	      "first person takes the whole offset - that camera has it at once");
	Check(Near(AimCameraShare(true, 0.4f, 0.1f), 0.1f),
	      "third person takes only the share the chase camera has reached");
}

void TestMeasuredChaseRate() {
	std::printf("The chase camera's rate, read off the camera\n");

	using obvr::camera::ChaseRateInput;
	using obvr::camera::kChaseDeltaMult;
	using obvr::camera::kChaseMinRemainingRadians;
	using obvr::camera::MeasuredChaseRate;

	const auto Near = [](float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; };

	// The headset's own numbers: the camera at -150.0 degrees, the body
	// written to -29.3 degrees further, the next frame's camera at -151.8.
	// That step is 6.1 percent of what was left - not the five the setting
	// says, and it is the measured number that is used.
	const float degrees = 3.14159265f / 180.0f;
	ChaseRateInput input;
	input.haveBefore = true;
	input.cameraBefore = -150.0f * degrees;
	input.target = (-150.0f - 29.3f) * degrees;
	input.cameraNow = -151.8f * degrees;
	Check(Near(MeasuredChaseRate(input), 1.8f / 29.3f), "the rate is what the camera moved over what it had left");

	// The frame after, in which the physics did not step: the camera stood
	// still, and the rate is zero - which is exactly the share of the turn the
	// camera took in that frame. This is the frame a modelled rate got wrong.
	input.cameraBefore = -151.8f * degrees;
	input.cameraNow = -151.9f * degrees;
	input.target = (-150.0f - 28.6f) * degrees;
	const float still = MeasuredChaseRate(input);
	Check(still >= 0.0f && still < 0.005f, "a frame the physics skipped reads as no rate at all");

	// Nothing to measure against.
	input.haveBefore = false;
	Check(Near(MeasuredChaseRate(input), kChaseDeltaMult), "no previous frame: the fallback stands in");
	input.haveBefore = true;

	// Already there: the division would be noise, so the fallback stands in
	// here too.
	input.cameraBefore = 1.0f;
	input.cameraNow = 1.0f;
	input.target = 1.0f + kChaseMinRemainingRadians * 0.5f;
	Check(Near(MeasuredChaseRate(input), kChaseDeltaMult), "a camera at its target reports the fallback");

	// The other way, or past the target: neither is the easing.
	input.cameraBefore = 0.0f;
	input.target = 0.5f;
	input.cameraNow = -0.1f;
	Check(Near(MeasuredChaseRate(input), 0.0f), "a camera moving away from its target is not easing");
	input.cameraNow = 0.7f;
	Check(Near(MeasuredChaseRate(input), 1.0f), "and one past its target is clamped to all of it");

	// Across the seam: a target on the far side of pi is a short way round,
	// and the rate reads the same as anywhere else.
	const float kPi = 3.14159265f;
	input.cameraBefore = kPi - 0.05f;
	input.target = -kPi + 0.15f;   // 0.2 rad further round
	input.cameraNow = -kPi + 0.05f;  // moved 0.1 of it
	Check(Near(MeasuredChaseRate(input), 0.5f), "a step across the seam measures by the short arc");
}

void TestAimPitchHold() {
	std::printf("The third person pitch, borrowed and given back\n");

	using obvr::camera::AimPitchHold;
	using obvr::camera::AimPitchHoldDecision;
	using obvr::camera::AimPitchHoldInput;
	using obvr::camera::NextAimPitchHold;

	const auto Near = [](float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; };

	// Not aiming: the field is the mouse's, nothing is written, no offset.
	AimPitchHold hold;
	AimPitchHoldInput input;
	input.enginePitch = 0.3f;
	AimPitchHoldDecision decision = NextAimPitchHold(hold, input);
	Check(!decision.write && !decision.next.held, "idle in third person writes nothing");
	Check(Near(decision.next.mouseTilt, 0.3f), "and the mouse's tilt is simply what the field holds");
	Check(Near(decision.offset, 0.0f), "nothing borrowed, no offset");
	hold = decision.next;

	// The shot begins: the gaze is written over the mouse, and the offset is
	// the difference - which is what the camera will ease towards on top.
	input.writeGaze = true;
	input.gazePitch = -0.2f;
	decision = NextAimPitchHold(hold, input);
	Check(decision.write && Near(decision.value, -0.2f), "aiming writes the gaze");
	Check(decision.next.held, "and the field is now held");
	Check(Near(decision.next.mouseTilt, 0.3f), "the mouse's tilt is remembered");
	Check(Near(decision.offset, -0.5f), "the offset is gaze minus mouse");
	hold = decision.next;

	// Next frame, no mouse movement: the engine left what was written. Still
	// aiming, still written, mouse unchanged.
	input.enginePitch = -0.2f;
	decision = NextAimPitchHold(hold, input);
	Check(decision.write && Near(decision.next.mouseTilt, 0.3f),
	      "a still mouse leaves its tilt where it was");
	hold = decision.next;

	// The mouse moves while the field is held. Measured: a written rotation
	// survives and the engine ADDS the movement to it, so the mouse's
	// contribution is the difference from what was last put there.
	input.enginePitch = -0.2f + 0.05f;
	decision = NextAimPitchHold(hold, input);
	Check(Near(decision.next.mouseTilt, 0.35f), "mouse movement while held is accumulated");
	Check(Near(decision.offset, -0.2f - 0.35f), "and the offset follows the mouse");
	hold = decision.next;

	// Released, waiting for the arrow to go: nothing written, the field keeps
	// what it has plus whatever the mouse adds, and the offset is measured
	// against the field as the engine now leaves it.
	input.writeGaze = false;
	input.enginePitch = -0.15f;
	decision = NextAimPitchHold(hold, input);
	Check(!decision.write && decision.next.held, "waiting for the shot: held, not written");
	Check(Near(decision.next.fieldNow, -0.15f), "the field is what the engine left");
	Check(Near(decision.next.mouseTilt, 0.40f),
	      "and the 0.05 the engine added since the last write is the mouse's");
	Check(Near(decision.offset, -0.15f - 0.40f), "so the offset is field minus mouse");
	hold = decision.next;

	// Another frame of waiting with the mouse moving: the movement goes to
	// the mouse's tilt, once, not cumulatively.
	input.enginePitch = -0.10f;
	decision = NextAimPitchHold(hold, input);
	Check(Near(decision.next.mouseTilt, 0.45f), "movement while waiting still reaches the mouse");
	hold = decision.next;

	// The shot is gone: the field is given back to the mouse, the hold ends,
	// the offset is zero and the camera eases home from here.
	input.returnDue = true;
	decision = NextAimPitchHold(hold, input);
	Check(decision.write && Near(decision.value, 0.45f), "the return writes the mouse's own tilt");
	Check(!decision.next.held && Near(decision.offset, 0.0f), "nothing held, nothing offset");
	hold = decision.next;

	// A return with nothing held is nothing.
	decision = NextAimPitchHold(hold, input);
	Check(!decision.write, "a return with nothing held writes nothing");

	// Writing and returning due on the same frame: the caller withholds the
	// write, so that flow is the return above. But a write asked for wins
	// here if both are passed, and the test says so rather than leaving it
	// to be discovered.
	input.writeGaze = true;
	input.returnDue = true;
	hold.held = true;
	hold.mouseTilt = 0.1f;
	hold.fieldNow = 0.1f;
	input.enginePitch = 0.1f;
	decision = NextAimPitchHold(hold, input);
	Check(decision.write && Near(decision.value, input.gazePitch) && decision.next.held,
	      "asked to write and return at once, the write is what happens");
}

void TestAimTiltCorrection() {
	std::printf("The third person camera's swing about its pivot, put back\n");

	using obvr::camera::AimTiltCorrection;
	using obvr::camera::AimTiltInput;
	using obvr::camera::kAimTiltMinCosine;
	using obvr::NiPoint3;

	const auto Near = [](float a, float b) { return a - b < 0.02f && b - a < 0.02f; };

	// The sphere the probe measured: feet at (2022.58, 4732.85, 61.16), the
	// camera 30 units off the axis at head height with the view level, and
	// with the view 69 degrees up the camera swung down to 11 units out and
	// 28 below the pivot - r sin(pitch) and r cos(pitch) to a hundredth.
	AimTiltInput input;
	input.feet = NiPoint3{2022.58f, 4732.85f, 61.16f};
	input.centreKnown = true;

	// Nothing to undo.
	input.cameraPosition = NiPoint3{1993.40f, 4739.80f, 180.96f};
	input.cameraSinPitch = 0.0f;
	input.pitchShare = 0.0f;
	NiPoint3 delta = AimTiltCorrection(input);
	Check(Near(delta.x, 0.0f) && Near(delta.y, 0.0f) && Near(delta.z, 0.0f),
	      "no share, no correction");

	// The camera as the probe found it fully swung up (line 171 of the
	// second run): pitch 1.19 rad, 11.07 out, 91.92 above the feet. If ALL
	// of that pitch were OBVR's share, putting it back must land the camera
	// where it stood level: 30 out, 119.8 up.
	input.cameraPosition = NiPoint3{2011.81f, 4735.42f, 153.06f};
	input.cameraSinPitch = 0.9294f;
	input.pitchShare = -1.1932f;  // asin(0.9294), as a rotX: negative is looking up
	delta = AimTiltCorrection(input);
	const NiPoint3 put = input.cameraPosition + delta;
	const float outX = put.x - input.feet.x;
	const float outY = put.y - input.feet.y;
	const float out = obvr::math::Sqrt(outX * outX + outY * outY);
	Check(out > 29.9f && out < 30.1f, "put back level, the camera stands the sphere's radius out");
	Check(put.z - input.feet.z > 119.6f && put.z - input.feet.z < 120.0f,
	      "and at the pivot's height, which is the level camera's");
	Check(Near(outX / out, -29.18f / 30.0f) && Near(outY / out, 6.96f / 30.0f),
	      "along the same bearing from the feet - the tilt does not turn the camera");

	// Half the share: halfway round the same arc, still on the sphere.
	input.pitchShare = -0.5966f;
	delta = AimTiltCorrection(input);
	const NiPoint3 half = input.cameraPosition + delta;
	const float hx = half.x - input.feet.x;
	const float hy = half.y - input.feet.y;
	const float hz = half.z - 180.94f;
	const float radius = obvr::math::Sqrt(hx * hx + hy * hy + hz * hz);
	Check(radius > 29.6f && radius < 30.0f, "half the share keeps the camera on the sphere");

	// The sign: the share is in the engine's convention, positive looking
	// DOWN. A head looking up writes a negative rotX, the camera swings down
	// below the pivot to look up at it, and putting that share back raises
	// the camera towards level. The other sign lowers it.
	input.pitchShare = -0.3f;
	delta = AimTiltCorrection(input);
	Check(delta.z > 0.0f, "the share of a gaze looking up is put back by raising the camera");
	input.pitchShare = 0.3f;
	delta = AimTiltCorrection(input);
	Check(delta.z < 0.0f, "and the share of a gaze looking down by lowering it");

	// The refusals.
	input.centreKnown = false;
	delta = AimTiltCorrection(input);
	Check(Near(delta.x, 0.0f) && Near(delta.z, 0.0f), "no feet, no correction");
	input.centreKnown = true;

	input.cameraPosition = input.feet;
	delta = AimTiltCorrection(input);
	Check(Near(delta.x, 0.0f) && Near(delta.z, 0.0f), "a camera on the axis has no arm to read");

	input.cameraPosition = NiPoint3{2011.81f, 4735.42f, 153.06f};
	input.cameraSinPitch = 0.9999f;
	delta = AimTiltCorrection(input);
	Check(Near(delta.x, 0.0f) && Near(delta.z, 0.0f),
	      "a camera nearly straight below the pivot is left alone rather than divided by a "
	      "cosine near zero");
	Check(kAimTiltMinCosine > 0.0f, "the limit exists");
}

void TestAimAtSource() {
	std::printf("The aim set at the source, and what the turn still owns\n");

	using obvr::camera::AimAtSourceWanted;
	using obvr::camera::AimSourceSwapDue;
	using obvr::camera::AimTurnOwnsAction;

	// The turn's ownership of actions. Off: the set IsShotUnreleased has
	// always used - Attack, AttackBow, ArrowAttached. On: nothing, the one
	// call the source aim wraps reads them all.
	Check(AimTurnOwnsAction(false, 2), "source off - Attack is the turn's");
	Check(AimTurnOwnsAction(false, 4), "source off - AttackBow is the turn's");
	Check(AimTurnOwnsAction(false, 5), "source off - ArrowAttached is the turn's");
	Check(!AimTurnOwnsAction(false, 3), "source off - FollowThrough never was");
	Check(!AimTurnOwnsAction(false, -1), "source off - no action, no turn");
	Check(!AimTurnOwnsAction(true, 2), "source on - Attack is no longer the turn's");
	Check(!AimTurnOwnsAction(true, 4), "source on - nor the bow");
	Check(!AimTurnOwnsAction(true, 5), "source on - nor the arrow on the string");

	// Every gate of the wanted flag, one at a time.
	Check(AimAtSourceWanted(true, true, true, false, true), "all gates open");
	Check(!AimAtSourceWanted(false, true, true, false, true), "aim disabled");
	Check(!AimAtSourceWanted(true, false, true, false, true), "source aim off");
	Check(!AimAtSourceWanted(true, true, false, false, true), "no headset");
	Check(!AimAtSourceWanted(true, true, true, true, true), "menu up");
	Check(!AimAtSourceWanted(true, true, true, false, false), "view not aimed (third person off)");

	// The swap per invocation of the key handler.
	Check(AimSourceSwapDue(true, true, 2), "the player's swing or cast is swapped");
	Check(AimSourceSwapDue(true, true, 5), "the player's bow release is swapped");
	Check(!AimSourceSwapDue(true, true, 4), "a key during the draw is not");
	Check(!AimSourceSwapDue(true, true, 3), "a key in the follow-through is not");
	Check(!AimSourceSwapDue(true, true, -1), "a footstep with nothing in flight is not");
	Check(!AimSourceSwapDue(true, true, 0), "an equip key is not");
	Check(!AimSourceSwapDue(true, false, 2), "an NPC's attack is not");
	Check(!AimSourceSwapDue(false, true, 2), "not wanted, not swapped");
}


int main() {
	TestChaseCamera();
	std::printf("\n");
	TestMeasuredChaseRate();
	std::printf("\n");
	TestAimPitchHold();
	std::printf("\n");
	TestAimTiltCorrection();
	std::printf("\n");
	TestAimAtSource();
	std::printf("\n");
	std::printf("OBVR frame logic test\n\n");

	TestKeyEdge();
	std::printf("\n");
	TestKeyEdgeReset();
	std::printf("\n");
	TestPointOfView();
	std::printf("\n");
	TestFirstPassInThirdPerson();
	std::printf("\n");
	TestIsDue();
	std::printf("\n");
	TestEyeAlternation();
	std::printf("\n");
	TestScaledEyeSeparation();
	std::printf("\n");
	TestBackBufferEye();
	std::printf("\n");
	TestFirstPassDrawsLeftEye();
	std::printf("\n");
	TestDeliverFrame();
	std::printf("\n");
	TestWorldlessBridge();
	std::printf("\n");
	TestMenuDressingWindow();
	std::printf("\n");
	TestMenuWorldProbe();
	std::printf("\n");
	TestWorldControlProbe();
	std::printf("\n");
	TestMenuLiveBackground();
	std::printf("\n");
	TestStereoEyeStep();
	std::printf("\n");
	TestAimTurnMode();
	std::printf("\n");
	TestAimReturn();
	std::printf("\n");
	TestCrosshair();
	TestBorrowedCrosshair();
	TestCrosshairCutout();
	TestCrosshairDepth();
	std::printf("\n");
	TestLayoutProbeDue();
	std::printf("\n");
	TestMenusCanReachTheWorld();
	std::printf("\n");
	TestWantsSecondScenePass();
	std::printf("\n");
	TestWantsHudRedirect();
	std::printf("\n");
	TestSweepProbeStage();
	std::printf("\n");
	TestSecondPassUnderProbe();
	std::printf("\n");
	TestDeliversDualEyes();
	std::printf("\n");
	TestAimPitchWanted();
	std::printf("\n");
	TestPlayerPitchForGaze();
	std::printf("\n");
	TestAtan2();
	std::printf("\n");
	TestWrapAngle();
	std::printf("\n");
	TestAimYaw();
	std::printf("\n");
	TestAimYawRemaining();
	std::printf("\n");
	TestReleaseClock();
	std::printf("\n");
	TestCastArm();
	std::printf("\n");
	TestCastWindow();
	std::printf("\n");
	TestAimArcCorrection();
	std::printf("\n");
	TestYawWriteLanded();
	std::printf("\n");
	TestAimYawHoldsTheView();
	std::printf("\n");
	TestFrameClock();
	std::printf("\n");
	TestFrameClockGaps();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
