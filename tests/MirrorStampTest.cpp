// Checks the skin build stamp across the mirror's passes (game/MirrorStampLogic.h,
// docs/mirror-spec.md 15): the palette builder's rule (a skin is built again
// at its draw when its stamp differs from the renderer's, then takes it), and
// the decision to count the renderer's stamp up before a second mirror pass in
// the same frame. A frame is played over plain values as the owner's log of
// 2026-11-28 14:56 measured it - the stamp moves between the eyes' render and
// the first mirror pass, and not between the two mirror passes ("skin stamp
// 79675" before both) - and every skin drawn in a pass must be built for that
// pass's camera: the player's head (its stamp turned around each pass, game/
// MirrorHead.cpp), the body (drawn by the eyes too), and an actor only the
// mirror sees.

#include <cstdio>

#include "game/MirrorHeadLogic.h"
#include "game/MirrorStampLogic.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

// A skin as the builder sees it: its stamp and the camera its rows were last
// built for.
struct Skin {
	std::uint32_t stamp = 0;
	int builtFor = -1;
};

enum Camera { kEyes = 0, kMirrorLeft = 1, kMirrorRight = 2 };

void Draw(Skin& skin, std::uint32_t rendererStamp, int camera) {
	if (BuildSkinIfStale(skin.stamp, rendererStamp)) {
		skin.builtFor = camera;
	}
}

// One mirror pass as render/MirrorGlassPass.cpp runs it: the stamp decision,
// the head shown (its stamps turned), the draws, the head hidden (turned again).
void MirrorPass(MirrorStampState& state, std::uint32_t sceneCall, std::uint32_t& rendererStamp, int camera,
                Skin& head, Skin& body, Skin* other) {
	if (MirrorPassNeedsNewStamp(state, sceneCall, rendererStamp)) {
		++rendererStamp;  // what the renderer's end of a scene frame does (0x00777A40)
	}
	NoteMirrorPassStamp(state, sceneCall, rendererStamp);
	head.stamp = StaleMirrorStamp(head.stamp);
	Draw(head, rendererStamp, camera);
	Draw(body, rendererStamp, camera);
	if (other != nullptr) {
		Draw(*other, rendererStamp, camera);
	}
	head.stamp = StaleMirrorStamp(head.stamp);
}

void TestBuilderRule() {
	std::printf("The palette builder's rule\n");
	std::uint32_t stamp = 7;
	Check(!BuildSkinIfStale(stamp, 7) && stamp == 7, "the renderer's stamp: not built again");
	Check(BuildSkinIfStale(stamp, 8) && stamp == 8, "another stamp: built, and takes the renderer's");
	Check(!BuildSkinIfStale(stamp, 8), "drawn again in the same stamp: not built");
}

void TestTwoPasses() {
	std::printf("A frame with both eyes' passes (the stamp still between them)\n");
	Skin head;
	Skin body;
	Skin other;
	std::uint32_t renderer = 79646;
	// The eyes' render: the body and the collapsed head built for the eyes;
	// the actor behind the player not drawn.
	Draw(head, renderer, kEyes);
	Draw(body, renderer, kEyes);
	renderer = 79675;  // the frame's other scene frames end before the mirror (measured)
	MirrorStampState state;
	MirrorPass(state, 3017, renderer, kMirrorLeft, head, body, &other);
	Check(head.builtFor == kMirrorLeft && body.builtFor == kMirrorLeft && other.builtFor == kMirrorLeft,
	      "the left eye's pass: every skin built for the left mirror camera");
	MirrorPass(state, 3017, renderer, kMirrorRight, head, body, &other);
	Check(head.builtFor == kMirrorRight, "the right eye's pass: the head (face, hair) built for the right camera");
	Check(body.builtFor == kMirrorRight, "the right eye's pass: the body built for the right camera");
	Check(other.builtFor == kMirrorRight, "the right eye's pass: an actor only the mirror sees built for the right camera");
	Check(renderer == 79676, "the renderer's stamp counted up once, before the second pass");

	// The next frame: the eyes' render builds the head from the collapse again.
	renderer = 79704;
	Draw(head, renderer, kEyes);
	Check(head.builtFor == kEyes, "the next frame's eyes: the head built again for them");
	MirrorPass(state, 3018, renderer, kMirrorLeft, head, body, &other);
	Check(renderer == 79704, "the next frame's first pass: no count (another scene call)");
	Check(head.builtFor == kMirrorLeft && other.builtFor == kMirrorLeft, "and built for its own camera");
}

void TestDecision() {
	std::printf("When the stamp is counted up\n");
	MirrorStampState s;
	Check(!MirrorPassNeedsNewStamp(s, 10, 500), "the first pass ever: no");
	NoteMirrorPassStamp(s, 10, 500);
	Check(MirrorPassNeedsNewStamp(s, 10, 500), "a second pass in the same scene call with the same stamp: yes");
	Check(!MirrorPassNeedsNewStamp(s, 10, 501), "the stamp moved by itself since: no");
	Check(!MirrorPassNeedsNewStamp(s, 11, 500), "another scene call (the next frame, taking turns, MirrorDepth=0): no");
	NoteMirrorPassStamp(s, 10, 501);
	Check(!MirrorPassNeedsNewStamp(s, 10, 500) && MirrorPassNeedsNewStamp(s, 10, 501),
	      "it compares with the stamp the last pass drew with, counted up or not");
	MirrorStampState unread;
	NoteMirrorPassStamp(unread, 12, 0);
	Check(!MirrorPassNeedsNewStamp(unread, 12, 0), "a stamp that could not be read (0): never counted");
}

void TestOnePassFrames() {
	std::printf("One pass a frame: never counted (stage 3, taking turns)\n");
	MirrorStampState state;
	Skin head;
	Skin body;
	std::uint32_t renderer = 1000;
	bool counted = false;
	for (std::uint32_t call = 1; call <= 6; ++call) {
		renderer += 29;
		Draw(head, renderer, kEyes);
		Draw(body, renderer, kEyes);
		const std::uint32_t before = renderer;
		MirrorPass(state, call, renderer, call % 2 == 0 ? kMirrorRight : kMirrorLeft, head, body, nullptr);
		counted = counted || renderer != before;
		Check(head.builtFor == (call % 2 == 0 ? kMirrorRight : kMirrorLeft), "the head built for the frame's one camera");
	}
	Check(!counted, "six frames of one pass: the renderer's stamp left alone");
}

}  // namespace

int main() {
	TestBuilderRule();
	TestTwoPasses();
	TestDecision();
	TestOnePassFrames();
	if (g_failures > 0) {
		std::printf("%d failing check(s)\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
