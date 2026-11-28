// Checks the holding hand's finger skin measured from its drawn vertices
// (vr/FingerSkin.h; docs/inventory-tablet-spec.md 32.2, the run of
// 2026-11-28 14:56): each vertex given to the nearest bone - a finger's, the
// palm's or the thumb's - each finger's skin as far as its vertices reach
// round its bones, a fingertip's pad past the tip included; each finger's
// deepest vertex against the page; the widest reading kept, within bounds;
// the limit held by the measured skin; and when the log says it.

#include <chrono>
#include <cstdio>

#include "vr/FingerSkin.h"

using namespace obvr;
using namespace obvr::vr;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float tolerance = 0.0005f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

// A hand at the origin, metres (one unit a metre): the four fingers along +x
// from knuckles 9 cm out, 2 cm apart in z; the thumb along -z.
HandBonesNow Bones() {
	HandBonesNow b;
	b.valid = true;
	b.wrist = NiPoint3{0.0f, 0.0f, 0.0f};
	for (int f = 0; f < 4; ++f) {
		const float z = 0.03f - 0.02f * static_cast<float>(f);
		b.fingerValid[f] = true;
		b.finger[f][0] = NiPoint3{0.09f, 0.0f, z};
		b.finger[f][1] = NiPoint3{0.135f, 0.0f, z};
		b.finger[f][2] = NiPoint3{0.16f, 0.0f, z};
		b.finger[f][3] = NiPoint3{0.18f, 0.0f, z};
	}
	b.thumbValid = true;
	b.thumb[0] = NiPoint3{0.02f, 0.0f, 0.04f};
	b.thumb[1] = NiPoint3{0.04f, 0.0f, 0.07f};
	b.thumb[2] = NiPoint3{0.05f, 0.0f, 0.09f};
	b.thumb[3] = NiPoint3{0.055f, 0.0f, 0.105f};
	return b;
}

SheetInWorld Sheet(float out) {
	SheetInWorld s;
	s.valid = true;
	s.centre = NiPoint3{0.1f, out, 0.0f};
	s.right = NiPoint3{1.0f, 0.0f, 0.0f};
	s.up = NiPoint3{0.0f, 0.0f, 1.0f};
	s.front = NiPoint3{0.0f, 1.0f, 0.0f};
	s.halfWidth = 0.2f;
	s.halfHeight = 0.2f;
	return s;
}

void TestMeasure() {
	std::printf("The skin measured round the bones:\n");
	const HandBonesNow b = Bones();
	NiPoint3 v[16];
	UInt32 n = 0;
	// The index (z 0.03): its skin 1.2 cm above its middle link, its pad 0.8 cm past the tip.
	v[n++] = NiPoint3{0.15f, 0.012f, 0.03f};
	v[n++] = NiPoint3{0.188f, 0.0f, 0.03f};
	// The middle finger (z 0.01): 0.9 cm round its first link.
	v[n++] = NiPoint3{0.11f, -0.009f, 0.01f};
	// The palm between wrist and knuckles, 1.5 cm above: the palm's, no finger's.
	v[n++] = NiPoint3{0.045f, 0.015f, 0.0f};
	// The thumb's ball: the thumb's.
	v[n++] = NiPoint3{0.04f, 0.012f, 0.07f};
	const FingerSkinRead r = ReadFingerSkin(v, n, b, Sheet(0.30f), 1.0f);
	Check(r.valid && r.measured[0] && Near(r.radius[0], 0.012f), "the index: 1.2 cm, its widest");
	Check(r.counted[0] == 2, "the pad past the tip counted to the index, from the tip");
	Check(r.measured[1] && Near(r.radius[1], 0.009f), "the middle finger: 0.9 cm");
	Check(!r.measured[2] && !r.measured[3], "the ring and the little finger: no vertex, not measured");
	Check(r.counted[0] + r.counted[1] == 3, "the palm's and the thumb's vertices given to no finger");
	Check(Near(r.deepest[0], 0.012f - 0.30f) && r.over[0], "the index's deepest point 28.8 cm behind a page 30 cm out");

	NiPoint3 through[1] = {NiPoint3{0.15f, 0.012f, 0.03f}};
	const FingerSkinRead t = ReadFingerSkin(through, 1, b, Sheet(0.005f), 1.0f);
	Check(t.over[0] && Near(t.deepest[0], 0.007f), "a page 5 mm out: the index's skin 7 mm through it");
	SheetInWorld aside = Sheet(0.005f);
	aside.centre.x = 1.0f;
	const FingerSkinRead a = ReadFingerSkin(through, 1, b, aside, 1.0f);
	Check(a.measured[0] && !a.over[0], "beside the page: measured, but over nothing");

	// Units: 70 a metre.
	NiPoint3 scaled[1] = {NiPoint3{0.15f * 70.0f, 0.012f * 70.0f, 0.03f * 70.0f}};
	HandBonesNow s = b;
	for (int f = 0; f < 4; ++f) {
		for (int j = 0; j < 4; ++j) {
			s.finger[f][j] = s.finger[f][j] * 70.0f;
		}
	}
	for (int j = 0; j < 4; ++j) {
		s.thumb[j] = s.thumb[j] * 70.0f;
	}
	SheetInWorld ss = Sheet(0.30f);
	ss.centre = ss.centre * 70.0f;
	ss.halfWidth *= 70.0f;
	ss.halfHeight *= 70.0f;
	const FingerSkinRead u = ReadFingerSkin(scaled, 1, s, ss, 70.0f);
	Check(Near(u.radius[0], 0.012f), "in the game's units: the same 1.2 cm");

	HandBonesNow none = b;
	none.valid = false;
	Check(!ReadFingerSkin(v, n, none, Sheet(0.3f), 1.0f).valid, "no bones: no reading");
	Check(!ReadFingerSkin(nullptr, 0, b, Sheet(0.3f), 1.0f).valid, "no vertices: no reading");
	HandBonesNow noThumb = b;
	noThumb.thumbValid = false;
	NiPoint3 ball[1] = {NiPoint3{0.04f, 0.012f, 0.07f}};
	const FingerSkinRead nt = ReadFingerSkin(ball, 1, noThumb, Sheet(0.3f), 1.0f);
	Check(!nt.measured[0], "the thumb's bones not read: its ball nearer the palm than any finger, still no finger's");
}

void TestKept() {
	std::printf("The widest reading kept:\n");
	FingerSkinKept k;
	Check(Near(SkinOf(k, 0), kFingerSkinMetres), "nothing measured: the 1 cm guess");
	FingerSkinRead r;
	r.valid = true;
	r.measured[0] = true;
	r.radius[0] = 0.014f;
	KeepFingerSkin(k, r);
	Check(Near(SkinOf(k, 0), 0.014f + kPageClearMetres), "measured: the skin and the page's clearance");
	r.radius[0] = 0.011f;
	KeepFingerSkin(k, r);
	Check(Near(SkinOf(k, 0), 0.014f + kPageClearMetres), "a narrower reading later (a bent joint's other side): the widest kept");
	r.radius[0] = 0.08f;
	KeepFingerSkin(k, r);
	Check(Near(SkinOf(k, 0), kFingerSkinMostMetres + kPageClearMetres), "8 cm: a misread, held to 2.5 cm");
	r.radius[0] = 0.001f;
	FingerSkinKept k2;
	KeepFingerSkin(k2, r);
	Check(Near(SkinOf(k2, 0), kFingerSkinLeastMetres + kPageClearMetres), "1 mm: held to 5 mm at the least");
	Check(Near(SkinOf(k2, 1), kFingerSkinMetres), "another finger unmeasured: the guess");
}

void TestLimitBySkin() {
	std::printf("The limit held by the measured skin:\n");
	HeldFingersInput in;
	in.holding = true;
	in.holderRight = true;
	in.sheet = Sheet(0.06f);
	in.bones.valid = true;
	in.bones.hand = NiTransform{NiMatrix33::Identity(), NiPoint3{0.0f, 0.0f, 0.0f}, 1.0f};
	for (int f = 0; f < 4; ++f) {
		in.bones.fingers[f].valid = true;
		in.bones.fingers[f].local[0] = NiPoint3{0.09f, 0.0f, 0.0f};
		in.bones.fingers[f].local[1] = NiPoint3{0.045f, 0.0f, 0.0f};
		in.bones.fingers[f].local[2] = NiPoint3{0.025f, 0.0f, 0.0f};
		in.rightCurl[f + 1] = 0.6f;
	}
	const HeldFingersOutput thin = HoldFingersBehind(in);
	in.skin[0] = 0.02f;
	const HeldFingersOutput wide = HoldFingersBehind(in);
	Check(wide.limit[0].limited && wide.rightCurl[1] != thin.rightCurl[1],
	      "the index's skin measured wider: held further from the page than the guess held it");
	Check(wide.rightCurl[2] == thin.rightCurl[2], "the middle finger by its own skin, unchanged");
}

void TestLine() {
	std::printf("When the log says it:\n");
	SkinLineState s;
	Check(SkinLineDue(s, true, 0x0, false), "the first reading of a hold: said");
	Check(!SkinLineDue(s, true, 0x0, false), "the same again: not");
	Check(SkinLineDue(s, true, 0x1, false), "the index through now: said");
	Check(SkinLineDue(s, true, 0x0, false), "and behind again: said");
	Check(SkinLineDue(s, true, 0x0, true), "at a mark: said");
	for (int i = 0; i < 10; ++i) {
		SkinLineDue(s, true, static_cast<UInt8>(i & 1), false);
	}
	Check(!SkinLineDue(s, true, 0x2, false), "a hold's lines used up: no more, however it changes");
	Check(!SkinLineDue(s, false, 0x0, false), "not held: nothing");
	Check(SkinLineDue(s, true, 0x0, false), "the next hold: said again");
}

// What a reading costs: the hand's 880 vertices (its count in Nadi's log),
// printed for the spec; read only when the reach is (every few frames at most).
void TestCost() {
	std::printf("The cost:\n");
	const HandBonesNow b = Bones();
	static NiPoint3 v[880];
	for (int i = 0; i < 880; ++i) {
		const float t = static_cast<float>(i) / 880.0f;
		v[i] = NiPoint3{0.2f * t, 0.01f * static_cast<float>(i % 7) - 0.03f, 0.08f * t - 0.04f};
	}
	const int runs = 200;
	float sink = 0.0f;
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < runs; ++i) {
		sink += ReadFingerSkin(v, 880, b, Sheet(0.05f), 1.0f).radius[0];
	}
	const double us =
		std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / runs;
	std::printf("  880 vertices: %.1f us a reading (%.2f)\n", us, static_cast<double>(sink));
	Check(us < 1000.0, "under a millisecond a reading");
}

}  // namespace

int main() {
	TestCost();
	TestMeasure();
	TestKept();
	TestLimitBySkin();
	TestLine();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
