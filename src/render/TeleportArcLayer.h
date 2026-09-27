#pragma once

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/OpenVRTypes.h"

namespace obvr::vr {
class OpenVRBackend;
}

namespace obvr::render {

// The teleport's arc and the ring where it lands (vr::Teleport), drawn as
// SteamVR overlays over the world, like the laser and the reach ring: the
// arc a chain of thin quads along the curve, the ring flat on the ground.
// Both in the reach ring's light brown, the parchment of Oblivion's menus -
// the tester asked for that colour instead of the usual green. A landing
// that is refused shows the same shapes dimmed and grey.
class TeleportArcLayer {
public:
	TeleportArcLayer() = default;
	TeleportArcLayer(const TeleportArcLayer&) = delete;
	TeleportArcLayer& operator=(const TeleportArcLayer&) = delete;

	static constexpr UInt32 kSegments = 16;

	// Once per frame. `points` are the arc's points in tracking space up to
	// where it landed (or ended), `count` of them; `eyes` the head's position
	// there. The ring is drawn at ringAt, lying on groundNormal, when
	// haveRing. Not visible hides everything.
	void Submit(vr::OpenVRBackend& backend, bool visible, const NiPoint3* points, UInt32 count,
	            const NiPoint3& eyes, bool haveRing, const NiPoint3& ringAt,
	            const NiPoint3& groundNormal, bool valid);

private:
	bool Ensure(vr::OpenVRBackend& backend);
	void HideAll(vr::OpenVRBackend& backend);
	void Tint(vr::OpenVRBackend& backend, bool valid);

	vr::openvr::VROverlayHandle m_segments[kSegments] = {};
	bool m_segmentShown[kSegments] = {};
	vr::openvr::VROverlayHandle m_ring = vr::openvr::kOverlayHandleInvalid;
	bool m_ringShown = false;
	bool m_created = false;
	bool m_tried = false;
	int m_tint = -1;  // 1 valid, 0 refused, -1 not set yet
	bool m_reported = false;
};

// The arc's texture: kArcTextureWidth by kArcTextureHeight, so a piece of
// length L is drawn L * width / height thick.
constexpr UInt32 kArcTextureWidth = 4;
constexpr UInt32 kArcTextureHeight = 128;
constexpr UInt32 kTeleportRingTexture = 64;
constexpr float kTeleportRingWidthMetres = 0.5f;

// Which of the points each drawn piece runs between: the points spread over
// the pieces as evenly as they go. Piece i of `pieces` runs from point
// first to point last. Pure, for the test.
struct ArcPiece {
	UInt32 first = 0;
	UInt32 last = 0;
};
inline UInt32 ArcPieceCount(UInt32 pointCount, UInt32 maxPieces) {
	if (pointCount < 2) {
		return 0;
	}
	const UInt32 segments = pointCount - 1;
	return segments < maxPieces ? segments : maxPieces;
}

inline ArcPiece ArcPieceAt(UInt32 index, UInt32 pieces, UInt32 pointCount) {
	ArcPiece piece;
	if (pieces == 0 || pointCount < 2) {
		return piece;
	}
	const UInt32 segments = pointCount - 1;
	piece.first = index * segments / pieces;
	piece.last = (index + 1) * segments / pieces;
	return piece;
}

}  // namespace obvr::render
