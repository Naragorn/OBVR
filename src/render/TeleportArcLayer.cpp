#include "render/TeleportArcLayer.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "vr/OpenVRBackend.h"
#include "vr/TeleportGeometry.h"

namespace obvr::render {
namespace {

// The reach ring's light brown (render::ReachMarker).
constexpr UInt8 kRed = 222;
constexpr UInt8 kGreen = 190;
constexpr UInt8 kBlue = 140;

// A line with soft sides, brighter towards the hand end.
void PaintArc(UInt8* rgba) {
	for (UInt32 row = 0; row < kArcTextureHeight; ++row) {
		for (UInt32 col = 0; col < kArcTextureWidth; ++col) {
			const bool core = col == 1 || col == 2;
			UInt8* const pixel = rgba + (row * kArcTextureWidth + col) * 4;
			pixel[0] = kRed;
			pixel[1] = kGreen;
			pixel[2] = kBlue;
			pixel[3] = core ? 230 : 90;
		}
	}
}

// The reach ring's shape, thicker: full between 0.7 and 0.9 of the radius.
void PaintRing(UInt8* rgba) {
	const float half = 0.5f * static_cast<float>(kTeleportRingTexture);
	for (UInt32 row = 0; row < kTeleportRingTexture; ++row) {
		for (UInt32 col = 0; col < kTeleportRingTexture; ++col) {
			const float dx = (static_cast<float>(col) + 0.5f - half) / half;
			const float dy = (static_cast<float>(row) + 0.5f - half) / half;
			const float r = math::Sqrt(dx * dx + dy * dy);
			float alpha = 0.0f;
			if (r >= 0.7f && r <= 0.9f) {
				alpha = 1.0f;
			} else if (r > 0.6f && r < 0.7f) {
				alpha = (r - 0.6f) / 0.1f;
			} else if (r > 0.9f && r < 1.0f) {
				alpha = (1.0f - r) / 0.1f;
			}
			UInt8* const pixel = rgba + (row * kTeleportRingTexture + col) * 4;
			pixel[0] = kRed;
			pixel[1] = kGreen;
			pixel[2] = kBlue;
			pixel[3] = static_cast<UInt8>(alpha * 255.0f);
		}
	}
}

}  // namespace

bool TeleportArcLayer::Ensure(vr::OpenVRBackend& backend) {
	if (m_created) {
		return true;
	}
	if (m_tried) {
		return false;
	}
	m_tried = true;
	static UInt8 arc[kArcTextureWidth * kArcTextureHeight * 4];
	static UInt8 ring[kTeleportRingTexture * kTeleportRingTexture * 4];
	PaintArc(arc);
	PaintRing(ring);
	for (UInt32 i = 0; i < kSegments; ++i) {
		m_segments[i] = vr::openvr::kOverlayHandleInvalid;
	}
	char key[32] = "obvr.teleport.arc.00";
	for (UInt32 i = 0; i < kSegments; ++i) {
		key[18] = static_cast<char>('0' + i / 10);
		key[19] = static_cast<char>('0' + i % 10);
		if (!backend.CreateOverlay(key, "OBVR teleport arc", m_segments[i]) ||
		    backend.SetOverlayRaw(m_segments[i], arc, kArcTextureWidth, kArcTextureHeight) !=
		        vr::openvr::kOverlayErrorNone) {
			OBVR_LOG("Teleport: the arc's overlays could not be made - no arc is drawn");
			return false;
		}
	}
	if (!backend.CreateOverlay("obvr.teleport.ring", "OBVR teleport ring", m_ring) ||
	    backend.SetOverlayRaw(m_ring, ring, kTeleportRingTexture, kTeleportRingTexture) !=
	        vr::openvr::kOverlayErrorNone) {
		OBVR_LOG("Teleport: the ring's overlay could not be made - no ring is drawn");
		return false;
	}
	backend.SetOverlayWidthInMetres(m_ring, kTeleportRingWidthMetres);
	m_created = true;
	return true;
}

void TeleportArcLayer::HideAll(vr::OpenVRBackend& backend) {
	for (UInt32 i = 0; i < kSegments; ++i) {
		if (m_segmentShown[i]) {
			backend.HideOverlay(m_segments[i]);
			m_segmentShown[i] = false;
		}
	}
	if (m_ringShown) {
		backend.HideOverlay(m_ring);
		m_ringShown = false;
	}
}

void TeleportArcLayer::Tint(vr::OpenVRBackend& backend, bool valid) {
	const int wanted = valid ? 1 : 0;
	if (wanted == m_tint) {
		return;
	}
	m_tint = wanted;
	// Refused: grey and faint. The texture carries the brown; the colour
	// multiplies it.
	const float shade = valid ? 1.0f : 0.55f;
	const float alpha = valid ? 0.95f : 0.45f;
	for (UInt32 i = 0; i < kSegments; ++i) {
		backend.SetOverlayColor(m_segments[i], shade, shade, shade);
		backend.SetOverlayAlpha(m_segments[i], alpha);
	}
	backend.SetOverlayColor(m_ring, shade, shade, shade);
	backend.SetOverlayAlpha(m_ring, alpha);
}

void TeleportArcLayer::Submit(vr::OpenVRBackend& backend, bool visible, const NiPoint3* points,
                              UInt32 count, const NiPoint3& eyes, bool haveRing,
                              const NiPoint3& ringAt, const NiPoint3& groundNormal, bool valid) {
	if (!visible || points == nullptr || count < 2) {
		if (m_created) {
			HideAll(backend);
		}
		return;
	}
	if (!Ensure(backend)) {
		return;
	}
	Tint(backend, valid);
	const UInt32 pieces = ArcPieceCount(count, kSegments);
	for (UInt32 i = 0; i < kSegments; ++i) {
		if (i >= pieces) {
			if (m_segmentShown[i]) {
				backend.HideOverlay(m_segments[i]);
				m_segmentShown[i] = false;
			}
			continue;
		}
		const ArcPiece piece = ArcPieceAt(i, pieces, count);
		const NiPoint3& a = points[piece.first];
		const NiPoint3& b = points[piece.last];
		const float length = math::Sqrt((b - a).LengthSquared());
		backend.SetOverlayTransformAbsolute(m_segments[i], vr::ArcSegmentMatrix(a, b, eyes));
		backend.SetOverlayWidthInMetres(
			m_segments[i], length * static_cast<float>(kArcTextureWidth) /
			                   static_cast<float>(kArcTextureHeight));
		if (!m_segmentShown[i]) {
			backend.ShowOverlay(m_segments[i]);
			m_segmentShown[i] = true;
		}
	}
	if (haveRing) {
		backend.SetOverlayTransformAbsolute(m_ring, vr::GroundRingMatrix(ringAt, groundNormal, eyes));
		if (!m_ringShown) {
			backend.ShowOverlay(m_ring);
			m_ringShown = true;
		}
	} else if (m_ringShown) {
		backend.HideOverlay(m_ring);
		m_ringShown = false;
	}
	if (!m_reported) {
		m_reported = true;
		OBVR_LOG("Teleport: the arc is shown (%u pieces, ring %s)", pieces,
		         haveRing ? (valid ? "valid" : "refused") : "none");
	}
}

}  // namespace obvr::render
