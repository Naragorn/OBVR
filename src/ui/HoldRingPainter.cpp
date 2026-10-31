#include "ui/HoldRingPainter.h"

namespace obvr::ui {
namespace {

constexpr render::Pixel Colour(UInt8 r, UInt8 g, UInt8 b, UInt8 a) {
	return SwapRedAndBlue(render::Pixel{r, g, b, a});
}

// Oblivion's menus: parchment, dark brown ink, a bronze rim and gold.
struct Rgba {
	float r, g, b, a;  // 0..255, a 0..1
};

constexpr Rgba kRim{58, 40, 22, 1.0f};          // the bronze's dark edge
constexpr Rgba kTrack{44, 32, 18, 0.62f};       // the band not yet filled
constexpr Rgba kGoldInner{166, 124, 64, 0.97f}; // the fill, darker inside
constexpr Rgba kGoldOuter{242, 214, 156, 0.97f};
constexpr Rgba kParchment{228, 209, 168, 0.94f};
constexpr Rgba kParchmentEdge{196, 170, 124, 0.94f};
constexpr Rgba kInk{70, 48, 26, 1.0f};

// How much of a pixel at distance d lies inside radius r: a pixel's width of
// soft edge, so the circles are not stair-stepped.
float Inside(float d, float r) {
	const float c = r - d + 0.5f;
	return c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
}

// Paints `top` over `under` with `cover` of its alpha.
Rgba Over(const Rgba& under, const Rgba& top, float cover) {
	const float a = top.a * cover;
	if (a <= 0.0f) {
		return under;
	}
	const float outA = a + under.a * (1.0f - a);
	if (outA <= 0.0f) {
		return Rgba{0, 0, 0, 0};
	}
	const auto mix = [&](float t, float u) { return (t * a + u * under.a * (1.0f - a)) / outA; };
	return Rgba{mix(top.r, under.r), mix(top.g, under.g), mix(top.b, under.b), outA};
}

Rgba Lerp(const Rgba& p, const Rgba& q, float t) {
	const float s = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	return Rgba{p.r + (q.r - p.r) * s, p.g + (q.g - p.g) * s, p.b + (q.b - p.b) * s, p.a + (q.a - p.a) * s};
}

// A bar with round ends: from (x0, y) to (x1, y), `half` thick each side.
float InBar(float x, float y, float x0, float x1, float barY, float half) {
	const float cx = x < x0 ? x0 : (x > x1 ? x1 : x);
	const float dx = x - cx;
	const float dy = y - barY;
	return Inside(math::Sqrt(dx * dx + dy * dy), half);
}

render::Pixel ToPixel(const Rgba& c) {
	const auto byte = [](float v) {
		return static_cast<UInt8>(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v + 0.5f));
	};
	return Colour(byte(c.r), byte(c.g), byte(c.b), byte(c.a * 255.0f));
}

}  // namespace

void PaintHoldRing(Canvas& canvas, const HoldRingView& view) {
	const SInt32 size = static_cast<SInt32>(kHoldRingCanvas);
	const float s = static_cast<float>(size);
	const float centre = (s - 1.0f) * 0.5f;
	const float outer = s * 0.5f - 1.5f;
	const float rim = s * 0.018f;
	const float bandInner = outer - s * 0.17f;
	const float disc = bandInner - s * 0.04f;
	const float discRim = s * 0.014f;
	// The menu sign, three bars.
	const float barHalfLength = disc * 0.46f;
	const float barHalf = s * 0.026f;
	const float barGap = disc * 0.30f;
	for (SInt32 y = 0; y < size; ++y) {
		for (SInt32 x = 0; x < size; ++x) {
			const float px = static_cast<float>(x);
			const float py = static_cast<float>(y);
			const float dx = px - centre;
			const float dy = py - centre;
			const float d = math::Sqrt(dx * dx + dy * dy);
			Rgba c{0, 0, 0, 0};
			// The ring: a dark rim, the band - gold where filled, dark track
			// elsewhere - and a dark rim again inside it.
			const float inRing = Inside(d, outer) * (1.0f - Inside(d, bandInner - rim));
			if (inRing > 0.0f) {
				c = Over(c, kRim, inRing);
				const float inBand = Inside(d, outer - rim) * (1.0f - Inside(d, bandInner));
				const float across = (d - bandInner) / (outer - rim - bandInner);
				const Rgba band = HoldRingArcCovers(dx, dy, view.fill) ? Lerp(kGoldInner, kGoldOuter, across) : kTrack;
				c = Over(c, band, inBand);
			}
			// The parchment disc with its edge and the sign.
			const float inDisc = Inside(d, disc);
			if (inDisc > 0.0f) {
				c = Over(c, kRim, inDisc);
				c = Over(c, Lerp(kParchment, kParchmentEdge, d / disc), Inside(d, disc - discRim));
				const float lx0 = centre - barHalfLength;
				const float lx1 = centre + barHalfLength;
				float sign = InBar(px, py, lx0, lx1, centre - barGap, barHalf);
				const float mid = InBar(px, py, lx0, lx1, centre, barHalf);
				const float low = InBar(px, py, lx0, lx1, centre + barGap, barHalf);
				sign = sign > mid ? sign : mid;
				sign = sign > low ? sign : low;
				c = Over(c, kInk, sign);
			}
			canvas.SetPixel(x, y, ToPixel(c));
		}
	}
}

void PaintHoldRingFor(Canvas& canvas, const void* view) {
	PaintHoldRing(canvas, *static_cast<const HoldRingView*>(view));
}

}  // namespace obvr::ui
