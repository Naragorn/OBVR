// Checks the mirror page's parts (vr/MirrorPage.h, vr/MirrorDust.h): the
// frame and the glass inside it, the Mirror tab's small drawn glass, the
// texture coordinates stage 2's reflection lands on, and the dust texture
// made once on the CPU.

#include <cstdio>
#include <vector>

#include "vr/MirrorDust.h"
#include "vr/MirrorPage.h"

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

bool Near(float a, float b, float tolerance = 0.001f) {
	const float d = a - b;
	return d < tolerance && d > -tolerance;
}

UiRect Rect(float left, float top, float right, float bottom) {
	UiRect r;
	r.left = left;
	r.top = top;
	r.right = right;
	r.bottom = bottom;
	r.valid = true;
	return r;
}

void TestPage() {
	std::printf("The mirror page\n");
	const MirrorPageRects page = MirrorPageOf(Rect(100.0f, 200.0f, 1100.0f, 1200.0f));
	Check(page.frame.valid && Near(page.frame.left, 100.0f) && Near(page.frame.top, 200.0f) &&
	          Near(page.frame.right, 1100.0f) && Near(page.frame.bottom, 1200.0f),
	      "the frame is the page's whole rectangle");
	Check(page.glass.valid && Near(page.glass.left, 100.0f + 1000.0f * kMirrorFrameShare) &&
	          Near(page.glass.right, 1100.0f - 1000.0f * kMirrorFrameShare) &&
	          Near(page.glass.top, 200.0f + 1000.0f * kMirrorFrameShare) &&
	          Near(page.glass.bottom, 1200.0f - 1000.0f * kMirrorFrameShare),
	      "the glass inside it, the frame the same width on every side");
	Check(kMirrorFrameShare > 0.02f && kMirrorFrameShare < 0.1f, "a frame, not a hairline and not most of the page");
	Check(!MirrorPageOf(UiRect{}).glass.valid && !MirrorPageOf(UiRect{}).frame.valid, "no page: no mirror page");
	Check(!MirrorPageOf(Rect(0.0f, 0.0f, 0.0f, 10.0f)).glass.valid, "a page without a width: nothing");
	Check(!MirrorPageOf(Rect(0.0f, 0.0f, 10.0f, 0.0f)).glass.valid, "a page without a height: nothing");
	Check(!MirrorPageOf(Rect(0.0f, 0.0f, 10.0f, -5.0f)).frame.valid, "an upside-down page: nothing");
}

void TestIcon() {
	std::printf("The tab's small drawn glass\n");
	const UiRect tab = Rect(810.0f, 110.0f, 1090.0f, 200.0f);
	const MirrorIconRects icon = MirrorIconIn(tab);
	Check(icon.rim.valid && icon.glass.valid, "a rim and a glass");
	Check(icon.rim.left >= tab.left && icon.rim.right <= tab.right && icon.rim.top >= tab.top && icon.rim.bottom <= tab.bottom,
	      "inside the tab");
	Check(icon.glass.left > icon.rim.left && icon.glass.right < icon.rim.right && icon.glass.top > icon.rim.top &&
	          icon.glass.bottom < icon.rim.bottom,
	      "the glass inside the rim");
	Check(Near(0.5f * (icon.rim.left + icon.rim.right), 0.5f * (tab.left + tab.right)) &&
	          Near(0.5f * (icon.rim.top + icon.rim.bottom), 0.5f * (tab.top + tab.bottom)),
	      "centred in the tab");
	Check(Near(icon.rim.bottom - icon.rim.top, 0.7f * (tab.bottom - tab.top)), "0.7 of the tab's height, as the other tabs' pictures");
	Check(!MirrorIconIn(UiRect{}).rim.valid && !MirrorIconIn(Rect(0.0f, 0.0f, 0.0f, 5.0f)).rim.valid, "no tab, or one without a width: nothing");
}

void TestGlassUv() {
	std::printf("Where the reflection lands\n");
	float u[4];
	float v[4];
	const float whole[4] = {0.0f, 0.0f, 1.0f, 1.0f};
	GlassTexCoords(whole, u, v);
	Check(u[0] == 1.0f && u[1] == 0.0f && u[2] == 1.0f && u[3] == 0.0f, "the picture flipped left to right: the top left shows its right edge");
	Check(v[0] == 0.0f && v[1] == 0.0f && v[2] == 1.0f && v[3] == 1.0f, "not flipped up and down");
	const float part[4] = {0.1f, 0.2f, 0.6f, 0.9f};
	GlassTexCoords(part, u, v);
	Check(Near(u[0], 0.6f) && Near(u[1], 0.1f) && Near(u[2], 0.6f) && Near(u[3], 0.1f) && Near(v[0], 0.2f) && Near(v[2], 0.9f),
	      "a part of the texture: its rectangle, the same way");
}

void TestDust() {
	std::printf("The dust texture\n");
	std::vector<UInt32> a(kMirrorDustSide * kMirrorDustSide, 0xDEADBEEFu);
	FillMirrorDust(a.data(), kMirrorDustSide);
	UInt32 clear = 0;
	UInt32 specks = 0;
	UInt32 most = 0;
	bool premultiplied = true;
	for (const UInt32 t : a) {
		const UInt32 alpha = t >> 24;
		const UInt32 r = (t >> 16) & 0xFF;
		const UInt32 g = (t >> 8) & 0xFF;
		const UInt32 b = t & 0xFF;
		clear += alpha == 0 ? 1u : 0u;
		specks += alpha > 0 ? 1u : 0u;
		most = alpha > most ? alpha : most;
		premultiplied = premultiplied && r <= alpha && g <= alpha && b <= alpha;
		if (alpha == 0 && t != 0) {
			premultiplied = false;
		}
	}
	Check(most > 0 && most <= 31, "light: no texel over 12 percent alpha (31 of 255)");
	Check(specks >= 300, "there is dust: at least a few hundred texels touched");
	Check(clear * 10 >= a.size() * 6, "mostly clear glass: at least 60 percent of the texels untouched");
	Check(premultiplied, "premultiplied: no colour over its alpha, clear texels fully empty");
	std::vector<UInt32> b(kMirrorDustSide * kMirrorDustSide, 0u);
	FillMirrorDust(b.data(), kMirrorDustSide);
	Check(a == b, "made again, the same: no clock, no random state");
	std::vector<UInt32> c(kMirrorDustSide * kMirrorDustSide, 0xCAFEBABEu);
	FillMirrorDust(nullptr, kMirrorDustSide);
	FillMirrorDust(c.data(), 0);
	Check(c[0] == 0xCAFEBABEu, "no buffer, or no side: nothing written, nothing fails");
	std::vector<UInt32> small(32u * 32u, 0u);
	FillMirrorDust(small.data(), 32);
	UInt32 touched = 0;
	for (const UInt32 t : small) {
		touched += (t >> 24) > 0 ? 1u : 0u;
	}
	Check(touched > 0, "another side: made at that size");
}

}  // namespace

int main() {
	TestPage();
	TestIcon();
	TestGlassUv();
	TestDust();
	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
