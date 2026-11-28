#pragma once

// The skin build stamp across the mirror's passes (docs/mirror-spec.md 15).
//
// The palette builder 0x007655F0 builds a skin's bone rows again at its draw
// only when the skin's stamp (NiSkinInstance +0x18) differs from the renderer's
// (the word at [[renderer+0x8B0]+0x3C], read 0x00765608/0x00765614), then
// writes the renderer's into the skin (0x00765638). The renderer's word is
// counted up only by 0x00777A40, called from the renderer's end of a scene
// frame 0x00762600 (vtable +0x134). A mirror render ends no scene frame on the
// owner's route: RenderScene begins and ends one only when the default
// group's flag +0x200 is 0, and it is 1 there (OBVR.log 2026-11-28 14:56: "skin
// stamp 79675, renderer frames open 1 0" before both eyes' passes, "moved
// between the two passes in 0 of N frames" in every cost line).
//
// So the second mirror pass of a frame draws every skin the first built with
// the first's rows - relative to the first mirror camera (the rows' origin is
// the render's camera, 0x00762640 writing 0x00B3F92C). The bone lock replaces
// the rows it can pair with the eyes' render and so hid it for the body; the
// rest stay a camera's distance off: the player's face and hair (their stamps
// turned around each pass by game/MirrorHead.cpp, and two turns in one stamp
// period cancel) and any actor only the mirror sees.
//
// The fix at that place: before a second mirror pass in the same frame with
// the same stamp, the renderer's stamp is counted up once, as the engine's own
// end of a scene frame does - the second pass is a render of its own and every
// skin it draws is built for its camera. One pass a frame (MirrorDepth=0, the
// eyes taking turns) never counts.
//
// Pure; covered by mirror_stamp_test. render/MirrorGlassPass.cpp reads and
// counts the renderer's word.

#include <cstdint>

namespace obvr::game {

// The builder's rule, for the tests' frames: true when the skin is built (and
// then carries the renderer's stamp).
inline bool BuildSkinIfStale(std::uint32_t& skinStamp, std::uint32_t rendererStamp) {
	if (skinStamp == rendererStamp) {
		return false;
	}
	skinStamp = rendererStamp;
	return true;
}

// The stamp the last mirror pass drew with, and in which scene call.
struct MirrorStampState {
	bool passRan = false;
	std::uint32_t sceneCall = 0;
	std::uint32_t stamp = 0;
};

// Before a mirror pass: count the renderer's stamp up first? Yes when a mirror
// pass already drew in this scene call with the stamp that stands now. A stamp
// that could not be read (0) is never counted.
inline bool MirrorPassNeedsNewStamp(const MirrorStampState& s, std::uint32_t sceneCall, std::uint32_t stampNow) {
	return stampNow != 0 && s.passRan && s.sceneCall == sceneCall && s.stamp == stampNow;
}

// The stamp a pass draws with (after any count).
inline void NoteMirrorPassStamp(MirrorStampState& s, std::uint32_t sceneCall, std::uint32_t stamp) {
	s.passRan = true;
	s.sceneCall = sceneCall;
	s.stamp = stamp;
}

}  // namespace obvr::game
