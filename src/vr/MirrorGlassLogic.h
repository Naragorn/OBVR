#pragma once

// The mirror's reflection, stage 2 (docs/mirror-spec.md 5, 8 and 12): the
// camera behind the glass, its frustum through the glass's corners, the size
// of the picture, and whether a frame renders one at all. Pure; covered by
// mirror_glass_test. The engine side is render/MirrorGlassPass.cpp.
//
// The camera is built the way the engine builds its water reflection's
// (WaterManager::RenderReflections 0x0049BEF0, read 2026-11-28): the eye
// reflected in the plane, the view direction and the up reflected, and the
// third axis made again by a cross product, so the camera stays a proper
// rotation. Its triangles keep their winding (that pass sets no cull mode)
// and the picture comes out flipped left to right, which the glass undoes
// (vr::GlassTexCoords). The glass is the near plane and the frustum runs
// through its four corners (Kooima, "Generalized Perspective Projection"),
// so the picture maps onto the glass with plain texture coordinates.
//
// Axes: the game's world (x east, y north, z up, right-handed), units. The
// NiCamera's rotation holds forward, up and right in its columns 0, 1, 2
// (render/WaterReprojection.h WaterRenderCameraFromBody, measured at the
// water reflection's entry). Frustum edges are tangents at unit distance,
// as the engine's NiFrustum keeps them (game/GameCamera.h).

#include <cstdint>

#include "core/MathFns.h"
#include "game/NiMath.h"

namespace obvr::vr {

inline float MirrorDot(const NiPoint3& a, const NiPoint3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline NiPoint3 MirrorCross(const NiPoint3& a, const NiPoint3& b) {
	return NiPoint3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline bool MirrorFinite(const NiPoint3& p) {
	const float s = p.x + p.y + p.z;
	return s == s && s - s == 0.0f;
}

// --------------------------------------------------------------- reflection

// A point reflected in the plane through q with the unit normal n.
inline NiPoint3 ReflectPoint(const NiPoint3& p, const NiPoint3& q, const NiPoint3& n) {
	return p - n * (2.0f * MirrorDot(p - q, n));
}

// A direction reflected in a plane with the unit normal n.
inline NiPoint3 ReflectDirection(const NiPoint3& d, const NiPoint3& n) { return d - n * (2.0f * MirrorDot(d, n)); }

inline NiMatrix33 MirrorColumns(const NiPoint3& a, const NiPoint3& b, const NiPoint3& c) {
	NiMatrix33 m{};
	m.data[0][0] = a.x;
	m.data[1][0] = a.y;
	m.data[2][0] = a.z;
	m.data[0][1] = b.x;
	m.data[1][1] = b.y;
	m.data[2][1] = b.z;
	m.data[0][2] = c.x;
	m.data[1][2] = c.y;
	m.data[2][2] = c.z;
	return m;
}

// A camera's rotation (the NiCamera's columns: forward, up, right) reflected
// in a plane with the unit normal n, kept a rotation as the water camera is:
// forward and up reflected, right = forward x up. A reflection of all three
// columns would be a mirror image (determinant -1), and the engine would then
// draw every triangle's back.
inline NiMatrix33 ReflectCameraRotation(const NiMatrix33& camera, const NiPoint3& n) {
	const NiPoint3 forward = ReflectDirection(NiPoint3{camera.data[0][0], camera.data[1][0], camera.data[2][0]}, n);
	const NiPoint3 up = ReflectDirection(NiPoint3{camera.data[0][1], camera.data[1][1], camera.data[2][1]}, n);
	return MirrorColumns(forward, up, MirrorCross(forward, up));
}

// ---------------------------------------------------------------- the glass

// A corner of the glass from the holding controller's frame (OpenVR's axes:
// x right, y up, z back; metres) into the world, as the tablet's pieces are
// drawn (render/TabletQuad.cpp ProjectCorners).
inline NiPoint3 GlassCornerInWorld(const NiPoint3& handPos, const NiMatrix33& handRot, float unitsPerMetre,
                                   const NiPoint3& corner) {
	const NiPoint3 local{corner.x, -corner.z, corner.y};
	return handPos + handRot * (local * unitsPerMetre);
}

// The glass's plane from its corners (top left, top right, bottom left,
// bottom right, as seen from its face): its centre, its right (top left to
// top right) and up (bottom left to top left, made square to the right),
// both unit, and its normal right x up - towards the side the glass is seen
// from. Not valid for an edge shorter than kMinGlassEdgeUnits, sides more
// than kGlassSquareCosine off a right angle, a fourth corner further than
// kGlassCornerShare of the larger side from where the other three put it, or
// a corner that is not a number.
struct GlassPlane {
	bool valid = false;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	NiPoint3 right{0.0f, 0.0f, 0.0f};
	NiPoint3 up{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 0.0f};
	float width = 0.0f;
	float height = 0.0f;
};

inline constexpr float kMinGlassEdgeUnits = 0.5f;  // 7 mm at 70 units a metre
inline constexpr float kGlassSquareCosine = 0.05f;
inline constexpr float kGlassCornerShare = 0.05f;

inline GlassPlane GlassPlaneOf(const NiPoint3 (&corners)[4]) {
	GlassPlane g;
	for (const NiPoint3& c : corners) {
		if (!MirrorFinite(c)) {
			return g;
		}
	}
	const NiPoint3 across = corners[1] - corners[0];
	const NiPoint3 upward = corners[0] - corners[2];
	const float width = math::Sqrt(across.LengthSquared());
	const float height = math::Sqrt(upward.LengthSquared());
	if (!(width >= kMinGlassEdgeUnits) || !(height >= kMinGlassEdgeUnits)) {
		return g;
	}
	const NiPoint3 right = across * (1.0f / width);
	const float slant = MirrorDot(upward, right) / height;
	if (slant > kGlassSquareCosine || slant < -kGlassSquareCosine) {
		return g;
	}
	const NiPoint3 fourth = corners[2] + across;
	const float larger = width > height ? width : height;
	const float off = math::Sqrt((fourth - corners[3]).LengthSquared());
	if (!(off <= kGlassCornerShare * larger)) {
		return g;
	}
	NiPoint3 up = upward - right * MirrorDot(upward, right);
	up = up * (1.0f / math::Sqrt(up.LengthSquared()));
	g.centre = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25f;
	g.right = right;
	g.up = up;
	g.normal = MirrorCross(right, up);
	g.width = width;
	g.height = height;
	g.valid = true;
	return g;
}

// -------------------------------------------------------- the mirror camera

// Ok, or why there is no camera: no glass, the eye on the glass's plane or
// behind it (nearer than kMinEyeDistanceUnits in front: the glass's back, or
// a camera inside the plane), or the glass seen edge-on (the cosine between
// its normal and the way to the eye under kMinFacingCosine, about 84 degrees).
enum class MirrorView : std::uint8_t { Ok, NoGlass, EyeBehind, EdgeOn };

inline constexpr float kMinEyeDistanceUnits = 1.0f;
inline constexpr float kMinFacingCosine = 0.1f;

// The camera behind the glass for one eye: where it stands (the eye
// reflected), its rotation (NiCamera columns: forward out of the glass
// towards the eye's side, the glass's up, right = forward x up - the glass's
// left), the frustum's tangents through the four corners and the near plane
// at the glass. facing and eyeDistance say how the eye saw it.
struct MirrorCamera {
	MirrorView view = MirrorView::NoGlass;
	NiPoint3 position{0.0f, 0.0f, 0.0f};
	NiMatrix33 rotation = NiMatrix33::Identity();
	float left = 0.0f;
	float right = 0.0f;
	float top = 0.0f;
	float bottom = 0.0f;
	float nearUnits = 0.0f;
	float facing = 0.0f;
	float eyeDistance = 0.0f;
};

inline MirrorCamera MirrorCameraFor(const GlassPlane& glass, const NiPoint3 (&corners)[4], const NiPoint3& eye) {
	MirrorCamera m;
	if (!glass.valid || !MirrorFinite(eye)) {
		return m;
	}
	const NiPoint3 toEye = eye - glass.centre;
	const float distance = MirrorDot(toEye, glass.normal);
	const float length = math::Sqrt(toEye.LengthSquared());
	m.eyeDistance = distance;
	m.facing = length > 0.0f ? distance / length : 0.0f;
	if (!(distance >= kMinEyeDistanceUnits)) {
		m.view = MirrorView::EyeBehind;
		return m;
	}
	if (!(m.facing >= kMinFacingCosine)) {
		m.view = MirrorView::EdgeOn;
		return m;
	}
	const NiPoint3 forward = glass.normal;
	const NiPoint3 up = glass.up;
	const NiPoint3 right = MirrorCross(forward, up);
	m.position = eye - glass.normal * (2.0f * distance);
	m.rotation = MirrorColumns(forward, up, right);
	bool first = true;
	for (const NiPoint3& corner : corners) {
		const NiPoint3 d = corner - m.position;
		const float z = MirrorDot(d, forward);
		const float x = MirrorDot(d, right) / z;
		const float y = MirrorDot(d, up) / z;
		if (first) {
			m.left = m.right = x;
			m.bottom = m.top = y;
			m.nearUnits = z;
			first = false;
			continue;
		}
		m.left = x < m.left ? x : m.left;
		m.right = x > m.right ? x : m.right;
		m.bottom = y < m.bottom ? y : m.bottom;
		m.top = y > m.top ? y : m.top;
		m.nearUnits = z < m.nearUnits ? z : m.nearUnits;
	}
	m.view = MirrorView::Ok;
	return m;
}

// ------------------------------------------------------- the glass on screen

// Where the glass was drawn into one eye's picture this frame (render/
// TabletQuad.cpp, the glass piece's projected corners): its box and the eye's
// viewport in the frame's pixels, and the eye's place in the world.
struct GlassOnScreen {
	bool drawn = false;
	float minX = 0.0f;
	float minY = 0.0f;
	float maxX = 0.0f;
	float maxY = 0.0f;
	float viewportX = 0.0f;
	float viewportY = 0.0f;
	float viewportWidth = 0.0f;
	float viewportHeight = 0.0f;
	NiPoint3 eye{0.0f, 0.0f, 0.0f};
};

// The glass's box over the eye's viewport anywhere.
inline bool GlassInView(const GlassOnScreen& s) {
	return s.drawn && s.maxX > s.viewportX && s.minX < s.viewportX + s.viewportWidth && s.maxY > s.viewportY &&
	       s.minY < s.viewportY + s.viewportHeight;
}

// The longest side of the glass's box in the eyes it was drawn into; 0 for none.
inline float GlassLongSidePixels(const GlassOnScreen (&eyes)[2]) {
	float longest = 0.0f;
	for (const GlassOnScreen& s : eyes) {
		if (!s.drawn) {
			continue;
		}
		const float w = s.maxX - s.minX;
		const float h = s.maxY - s.minY;
		longest = w > longest ? w : longest;
		longest = h > longest ? h : longest;
	}
	return longest;
}

// The eye the one reflection of this stage is made for: between the eyes the
// glass was drawn into, or the one eye; false with none.
inline bool CyclopeanEye(const GlassOnScreen (&eyes)[2], NiPoint3& out) {
	if (eyes[0].drawn && eyes[1].drawn) {
		out = (eyes[0].eye + eyes[1].eye) * 0.5f;
		return true;
	}
	if (eyes[0].drawn || eyes[1].drawn) {
		out = eyes[0].drawn ? eyes[0].eye : eyes[1].eye;
		return true;
	}
	return false;
}

// -------------------------------------------------------------- the picture

// [Tablet] MirrorPixels: the mirror's texture is that many pixels a side, and
// no picture is larger.
inline constexpr std::uint32_t kMirrorPixelsMin = 256;
inline constexpr std::uint32_t kMirrorPixelsMax = 2048;
inline constexpr std::uint32_t kMirrorPixelsDefault = 1024;

inline std::uint32_t ClampMirrorPixels(float wanted) {
	if (!(wanted == wanted)) {
		return kMirrorPixelsDefault;
	}
	if (wanted < static_cast<float>(kMirrorPixelsMin)) {
		return kMirrorPixelsMin;
	}
	if (wanted > static_cast<float>(kMirrorPixelsMax)) {
		return kMirrorPixelsMax;
	}
	return static_cast<std::uint32_t>(wanted + 0.5f);
}

struct MirrorPictureSize {
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};

inline constexpr std::uint32_t kMirrorSideMin = 64;
inline constexpr std::uint32_t kMirrorShortSideMin = 16;

// The picture's size this frame: its long side the glass's long side on
// screen (as many pixels as the glass covers in the eye, rounded up to 8),
// no less than kMirrorSideMin and no more than the texture (cap); the short
// side from the glass's shape (aspect, width over height; square when not
// known); both scaled down together to fit the frame it is drawn into. None
// for a frame of no size.
inline MirrorPictureSize MirrorPictureSizeFor(float longSidePixels, float aspect, std::uint32_t cap,
                                              std::uint32_t frameWidth, std::uint32_t frameHeight) {
	MirrorPictureSize s;
	if (frameWidth == 0 || frameHeight == 0 || cap == 0) {
		return s;
	}
	if (!(aspect > 0.0f) || aspect > 1.0e6f) {
		aspect = 1.0f;
	}
	std::uint32_t longSide = kMirrorSideMin;
	if (longSidePixels > static_cast<float>(cap)) {
		longSide = cap;
	} else if (longSidePixels > static_cast<float>(kMirrorSideMin)) {
		longSide = (static_cast<std::uint32_t>(longSidePixels + 0.999f) + 7u) / 8u * 8u;
	}
	longSide = longSide > cap ? cap : longSide;
	float w = aspect >= 1.0f ? static_cast<float>(longSide) : static_cast<float>(longSide) * aspect;
	float h = aspect >= 1.0f ? static_cast<float>(longSide) / aspect : static_cast<float>(longSide);
	float scale = 1.0f;
	if (w > static_cast<float>(frameWidth)) {
		scale = static_cast<float>(frameWidth) / w;
	}
	if (h * scale > static_cast<float>(frameHeight)) {
		scale = static_cast<float>(frameHeight) / h;
	}
	w *= scale;
	h *= scale;
	s.width = static_cast<std::uint32_t>(w + 0.5f);
	s.height = static_cast<std::uint32_t>(h + 0.5f);
	s.width = s.width < kMirrorShortSideMin ? kMirrorShortSideMin : s.width;
	s.height = s.height < kMirrorShortSideMin ? kMirrorShortSideMin : s.height;
	s.width = s.width > frameWidth ? frameWidth : s.width;
	s.height = s.height > frameHeight ? frameHeight : s.height;
	return s;
}

// The main camera's port for the picture: the frame's top left corner, the
// picture's size (NiCamera +0x110: left, right, top, bottom as fractions of
// the target, top 1 and bottom 0 the whole height - render/ObngStereoLogic.h).
struct MirrorPort {
	float left = 0.0f;
	float right = 1.0f;
	float top = 1.0f;
	float bottom = 0.0f;
};

inline MirrorPort MirrorPortFor(MirrorPictureSize size, std::uint32_t frameWidth, std::uint32_t frameHeight) {
	MirrorPort p;
	if (frameWidth == 0 || frameHeight == 0) {
		return p;
	}
	p.right = static_cast<float>(size.width) / static_cast<float>(frameWidth);
	p.bottom = 1.0f - static_cast<float>(size.height) / static_cast<float>(frameHeight);
	return p;
}

// The part of the texture (textureSide a side) the picture fills, for the
// glass (render::MirrorGlassUv: left, top, right, bottom).
inline void MirrorGlassUvFor(MirrorPictureSize size, std::uint32_t textureSide, float (&rect)[4]) {
	const float side = textureSide > 0 ? static_cast<float>(textureSide) : 1.0f;
	rect[0] = 0.0f;
	rect[1] = 0.0f;
	rect[2] = static_cast<float>(size.width) / side;
	rect[3] = static_cast<float>(size.height) / side;
}

// ----------------------------------------------------------- this frame?

// A frame with the mirror's page up renders the reflection (None) or says
// why not, in this order: the setting off ([Tablet] Mirror or MirrorWorld),
// the pass switched off for the session, a load, the eyes not both captured
// this frame on a stereo frame (the pass draws over the back buffer, which is
// safe only once both eye pictures are copied), no glass, the eye behind it,
// edge-on, and the glass out of both eyes' view.
enum class MirrorSkip : std::uint8_t {
	None,
	Off,
	Stopped,
	Loading,
	NotCaptured,
	NoGlass,
	FacingAway,
	EdgeOn,
	OutOfView,
	Count
};

inline const char* MirrorSkipName(MirrorSkip s) {
	switch (s) {
	case MirrorSkip::None: return "rendered";
	case MirrorSkip::Off: return "switched off";
	case MirrorSkip::Stopped: return "stopped";
	case MirrorSkip::Loading: return "loading";
	case MirrorSkip::NotCaptured: return "eyes not captured";
	case MirrorSkip::NoGlass: return "no glass";
	case MirrorSkip::FacingAway: return "facing away";
	case MirrorSkip::EdgeOn: return "edge-on";
	case MirrorSkip::OutOfView: return "out of view";
	case MirrorSkip::Count: break;
	}
	return "unknown";
}

struct MirrorPassInputs {
	bool settingOn = false;
	bool loading = false;
	bool eyesCaptured = false;
	bool stopped = false;
	MirrorView view = MirrorView::NoGlass;
	bool inView = false;
};

inline MirrorSkip DecideMirrorPass(const MirrorPassInputs& in) {
	if (!in.settingOn) {
		return MirrorSkip::Off;
	}
	if (in.stopped) {
		return MirrorSkip::Stopped;
	}
	if (in.loading) {
		return MirrorSkip::Loading;
	}
	if (!in.eyesCaptured) {
		return MirrorSkip::NotCaptured;
	}
	switch (in.view) {
	case MirrorView::NoGlass: return MirrorSkip::NoGlass;
	case MirrorView::EyeBehind: return MirrorSkip::FacingAway;
	case MirrorView::EdgeOn: return MirrorSkip::EdgeOn;
	case MirrorView::Ok: break;
	}
	return in.inView ? MirrorSkip::None : MirrorSkip::OutOfView;
}

// Whether a single-pass frame's world render keeps its bone rows for the
// mirror's (render/SceneRenderHook.cpp): only while the page is up, the
// setting on and the pass not switched off. Decided before the render, so a
// frame that then skips the mirror (out of view) kept them for nothing.
inline bool MirrorWantsBoneCapture(bool pageUp, bool settingOn, bool stopped) {
	return pageUp && settingOn && !stopped;
}

// ------------------------------------------------------- the failure budget

// Failures in a row (a render that did not land in the frame, a copy refused)
// switch the pass off for the session after kMirrorFailureBudget; a success
// forgets the run. A lost device or a texture that could not be made switches
// it off at once (StopMirror). Both answer true exactly once, when the line
// that says so is due.
inline constexpr std::uint32_t kMirrorFailureBudget = 3;

struct MirrorFailures {
	std::uint32_t inARow = 0;
	bool stopped = false;
};

inline bool NoteMirrorOutcome(MirrorFailures& f, bool ok) {
	if (f.stopped) {
		return false;
	}
	if (ok) {
		f.inARow = 0;
		return false;
	}
	++f.inARow;
	if (f.inARow >= kMirrorFailureBudget) {
		f.stopped = true;
		return true;
	}
	return false;
}

inline bool StopMirror(MirrorFailures& f) {
	if (f.stopped) {
		return false;
	}
	f.stopped = true;
	return true;
}

// ------------------------------------------------------------ the cost line

// Every kMirrorCostEveryFrames frames with the mirror's page up, one log line
// (render/MirrorGlassPass.cpp): the passes, their mean and worst CPU time,
// the draws a pass, the picture's last size, the frames skipped by reason.
inline constexpr std::uint32_t kMirrorCostEveryFrames = 90;

struct MirrorCostWindow {
	std::uint32_t frames = 0;
	std::uint32_t passes = 0;
	double totalMs = 0.0;
	double worstMs = 0.0;
	std::uint64_t draws = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	std::uint32_t skipped[static_cast<int>(MirrorSkip::Count)] = {};
	// Stage 3: the passes that asked for the head and showed it, and the
	// switch's time (game/MirrorHead.h).
	std::uint32_t headAsked = 0;
	std::uint32_t headShown = 0;
	double headMs = 0.0;
};

// One pass's head switch: shown or refused, and what the switch cost.
inline void NoteMirrorHead(MirrorCostWindow& w, bool shown, double ms) {
	++w.headAsked;
	w.headShown += shown ? 1u : 0u;
	w.headMs += ms;
}

inline double MeanHeadMs(const MirrorCostWindow& w) {
	return w.headAsked > 0 ? w.headMs / static_cast<double>(w.headAsked) : 0.0;
}

// One frame with the page up: a pass (skip None, its milliseconds, draws and
// size) or a skip. True when the window is full and its line is due.
inline bool NoteMirrorFrame(MirrorCostWindow& w, MirrorSkip skip, double ms, std::uint32_t draws,
                            MirrorPictureSize size) {
	++w.frames;
	if (skip == MirrorSkip::None) {
		++w.passes;
		w.totalMs += ms;
		w.worstMs = ms > w.worstMs ? ms : w.worstMs;
		w.draws += draws;
		w.width = size.width;
		w.height = size.height;
	} else if (skip != MirrorSkip::Count) {
		++w.skipped[static_cast<int>(skip)];
	}
	return w.frames >= kMirrorCostEveryFrames;
}

inline double MeanMs(const MirrorCostWindow& w) {
	return w.passes > 0 ? w.totalMs / static_cast<double>(w.passes) : 0.0;
}

inline std::uint32_t SkippedTotal(const MirrorCostWindow& w) {
	std::uint32_t total = 0;
	for (std::uint32_t n : w.skipped) {
		total += n;
	}
	return total;
}

inline void ResetMirrorCost(MirrorCostWindow& w) { w = MirrorCostWindow{}; }

// ----------------------------------------------------------- the stage trace

// The first frames that render a reflection log each stage of the pass, so a
// log that ends in a crash or a lost device names the stage (the dual pass's
// trace, CameraHook.cpp DualTraceOn).
inline constexpr std::uint32_t kMirrorTraceFrames = 3;

struct MirrorTrace {
	std::uint32_t framesLeft = kMirrorTraceFrames;
	bool On() const { return framesLeft > 0; }
	void FrameDone() {
		if (framesLeft > 0) {
			--framesLeft;
		}
	}
};

}  // namespace obvr::vr
