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

#include <cstdio>
#include <limits>

#include "camera/FrameLogic.h"

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

int main() {
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
