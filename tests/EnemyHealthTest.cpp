// Checks the enemy health bars over the fighters (game/EnemyHealthLogic.h):
// the settings as used, the bar's width and point, the fraction, when a bar
// is painted anew, and every flow of StepEnemyHealthBars - a fighter's bar
// begun, kept, lingering after death or the end of the fight, let go, the
// nearest first into the slots, none past the slots. And the painter
// (ui/EnemyHealthPainter.h): the fill as wide as the health. And every flow
// of the lookup of a frame's Direct3D texture, which reads nothing but the
// three dwords it names (the crash of 2026-10-14), and the probe's line.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include "game/EnemyHealthLogic.h"
#include "ui/DxtDecode.h"
#include "ui/EnemyHealthPainter.h"

namespace {

using namespace obvr::game;
using obvr::NiPoint3;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 1e-3f) { return std::fabs(a - b) <= within; }

const float kNaN = std::numeric_limits<float>::quiet_NaN();

EnemyHealthSeen Fighter(UInt32 actor, float fraction, float metres, bool fighting = true, bool dead = false) {
	EnemyHealthSeen s;
	s.actor = actor;
	s.fighting = fighting;
	s.dead = dead;
	s.fraction = fraction;
	s.metres = metres;
	s.at = NiPoint3{metres * 70.0f, 0.0f, 140.0f};
	return s;
}

void TestRules() {
	std::printf("The rules\n");
	const EnemyHealthSettings g;
	Check(g.enabled && Near(g.rangeMetres, 25.0f) && Near(g.widthMetres, 0.35f), "the defaults: on, 25 m, 0.35 m");
	Check(Near(EnemyHealthRangeMetres(10.0f), 10.0f) && Near(EnemyHealthRangeMetres(1.0f), 1.0f) &&
	          Near(EnemyHealthRangeMetres(100.0f), 100.0f),
	      "the range as set, 1 to 100");
	Check(Near(EnemyHealthRangeMetres(0.0f), 25.0f) && Near(EnemyHealthRangeMetres(101.0f), 25.0f) &&
	          Near(EnemyHealthRangeMetres(kNaN), 25.0f),
	      "the range out of range or not a number: 25");
	Check(Near(EnemyHealthWidthMetres(0.5f), 0.5f) && Near(EnemyHealthWidthMetres(0.1f), 0.1f) &&
	          Near(EnemyHealthWidthMetres(2.0f), 2.0f),
	      "the width as set, 0.1 to 2");
	Check(Near(EnemyHealthWidthMetres(0.0f), 0.35f) && Near(EnemyHealthWidthMetres(3.0f), 0.35f) &&
	          Near(EnemyHealthWidthMetres(kNaN), 0.35f),
	      "the width out of range or not a number: 0.35");
	Check(Near(EnemyHealthBarWidth(0.35f, 2.0f), 0.35f), "at 2 m: the set width");
	Check(Near(EnemyHealthBarWidth(0.35f, 10.0f), 0.6f), "at 10 m: the minimum apparent size, 0.6 m");
	Check(Near(EnemyHealthBarWidth(0.35f, 0.0f), 0.35f), "at 0 m: the set width");
	Check(Near(EnemyHealthFraction(50.0f, 100.0f), 0.5f), "half the base: 0.5");
	Check(Near(EnemyHealthFraction(120.0f, 100.0f), 1.0f), "over the base (fortified): 1");
	Check(Near(EnemyHealthFraction(0.0f, 100.0f), 0.0f) && Near(EnemyHealthFraction(-5.0f, 100.0f), 0.0f),
	      "none or below: 0");
	Check(Near(EnemyHealthFraction(30.0f, 0.0f), 1.0f), "no base, health left: full");
	Check(Near(EnemyHealthFraction(0.0f, 0.0f), 0.0f), "no base, no health: empty");
	Check(Near(EnemyHealthFraction(kNaN, 100.0f), 0.0f), "health not a number: empty");
	Check(Near(EnemyHealthFraction(30.0f, kNaN), 1.0f), "base not a number, health left: full");
	const NiPoint3 head{10.0f, 20.0f, 120.0f};
	const NiPoint3 feet{10.0f, 20.0f, 0.0f};
	NiPoint3 at = EnemyHealthBarPoint(true, head, feet, 1.0f, 14.0f);
	Check(Near(at.x, 10.0f) && Near(at.y, 20.0f) && Near(at.z, 134.0f), "with the head bone: the raise above it");
	at = EnemyHealthBarPoint(false, head, feet, 1.0f, 14.0f);
	Check(Near(at.z, 136.0f), "without: the head's 122 units above the feet and the raise");
	at = EnemyHealthBarPoint(true, head, feet, 1.5f, 14.0f);
	Check(Near(at.z, 141.0f), "a scaled skeleton: the raise scaled");
	at = EnemyHealthBarPoint(false, head, feet, 1.5f, 14.0f);
	Check(Near(at.z, 204.0f), "a scaled skeleton without the bone: the height and the raise scaled");
	at = EnemyHealthBarPoint(false, head, feet, 0.0f, 14.0f);
	Check(Near(at.z, 136.0f), "a scale of 0: taken as 1");
	at = EnemyHealthBarPoint(false, head, feet, 50.0f, 14.0f);
	Check(Near(at.z, 136.0f), "a scale of 50: taken as 1");
	at = EnemyHealthBarPoint(true, head, feet, 1.0f, 22.4f);
	Check(Near(at.z, 142.4f), "the default raise (0.32 m, 22.4 units): above the bone");
	at = EnemyHealthBarPoint(true, head, feet, 1.0f, 0.0f);
	Check(Near(at.z, 120.0f), "no raise: on the bone");
	at = EnemyHealthBarPoint(true, head, feet, 1.0f, kNaN);
	Check(Near(at.z, 120.0f), "a raise that is not a number: on the bone");
	at = EnemyHealthBarPoint(true, head, feet, 1.0f, -5.0f);
	Check(Near(at.z, 120.0f), "a negative raise: on the bone");
	at = EnemyHealthBarPoint(false, head, feet, 1.0f, 1000.0f);
	Check(Near(at.z, 122.0f), "an absurd raise without the bone: the head's height only");
	Check(Near(EnemyHealthAboveMetres(0.32f), 0.32f) && Near(EnemyHealthAboveMetres(0.05f), 0.05f) &&
	          Near(EnemyHealthAboveMetres(1.0f), 1.0f),
	      "the raise as set, 0.05 to 1 m");
	Check(Near(EnemyHealthAboveMetres(0.01f), 0.32f) && Near(EnemyHealthAboveMetres(1.5f), 0.32f) &&
	          Near(EnemyHealthAboveMetres(kNaN), 0.32f),
	      "out of range or not a number: 0.32 m");
	Check(!EnemyHealthRepaintDue(0.5f, 0.5f), "the same fraction: no repaint");
	Check(!EnemyHealthRepaintDue(0.5f, 0.503f), "a hair's move: no repaint");
	Check(EnemyHealthRepaintDue(0.5f, 0.49f), "a 128th's move: repaint");
	Check(EnemyHealthRepaintDue(0.001f, 0.0f), "to empty from a hair: repaint (empty looks different)");
}

void TestBarFlows() {
	std::printf("The bars\n");
	EnemyHealthState s;
	std::vector<EnemyHealthSeen> seen;
	seen.push_back(Fighter(0xA, 1.0f, 3.0f));
	EnemyHealthVerdict v = StepEnemyHealthBars(s, seen.data(), 1, 0.02f);
	Check(v.started == 1 && v.shown == 1 && v.ended == 0, "a fighter: a bar begun");
	Check(s.bar[0].actor == 0xA && Near(s.bar[0].fraction, 1.0f) && s.bar[0].revision == 1 &&
	          Near(s.bar[0].painted, 1.0f) && Near(s.bar[0].metres, 3.0f),
	      "the bar: the fighter, full, painted once");
	Check(EnemyHealthBarOf(s, 0xA) == 0 && EnemyHealthBarOf(s, 0xB) < 0 && EnemyHealthBarOf(s, 0) < 0,
	      "the bar found by its fighter; nobody else; 0 never");

	seen[0] = Fighter(0xA, 0.5f, 2.5f);
	v = StepEnemyHealthBars(s, seen.data(), 1, 0.02f);
	Check(v.started == 0 && v.shown == 1 && s.bar[0].revision == 2 && Near(s.bar[0].painted, 0.5f) &&
	          Near(s.bar[0].metres, 2.5f) && Near(s.bar[0].seconds, 0.02f),
	      "hurt: the bar kept, painted anew, nearer, its time counted");

	seen[0] = Fighter(0xA, 0.501f, 2.5f);
	v = StepEnemyHealthBars(s, seen.data(), 1, 0.02f);
	Check(s.bar[0].revision == 2 && Near(s.bar[0].fraction, 0.501f) && Near(s.bar[0].painted, 0.5f),
	      "a hair's change: the fraction kept, not painted anew");

	// Dead: the bar at nothing, lingering, then gone.
	seen[0] = Fighter(0xA, 0.3f, 2.5f, false, true);
	v = StepEnemyHealthBars(s, seen.data(), 1, 0.5f);
	Check(v.shown == 1 && v.ended == 0 && Near(s.bar[0].fraction, 0.0f) && s.bar[0].dead &&
	          Near(s.bar[0].linger, 0.5f) && s.bar[0].revision == 3,
	      "dead: the bar empty, painted anew, lingering");
	v = StepEnemyHealthBars(s, seen.data(), 1, 1.0f);
	Check(v.shown == 1 && Near(s.bar[0].linger, 1.5f), "still lingering at 1.5 s");
	v = StepEnemyHealthBars(s, seen.data(), 1, 0.5f);
	Check(v.shown == 0 && v.ended == 1 && s.bar[0].actor == 0, "at 2 s: the bar gone");

	// Not seen at all (out of range, left the list): lingering with the
	// last fraction, then gone.
	s = EnemyHealthState{};
	seen[0] = Fighter(0xB, 0.7f, 4.0f);
	StepEnemyHealthBars(s, seen.data(), 1, 0.02f);
	v = StepEnemyHealthBars(s, seen.data(), 0, 1.0f);
	Check(v.shown == 1 && Near(s.bar[0].fraction, 0.7f) && Near(s.bar[0].linger, 1.0f),
	      "not seen: the bar lingers with its last health");
	v = StepEnemyHealthBars(s, seen.data(), 0, 1.0f);
	Check(v.shown == 0 && v.ended == 1, "after the linger: gone");

	// Fighting again within the linger: the linger reset.
	s = EnemyHealthState{};
	seen[0] = Fighter(0xB, 0.7f, 4.0f);
	StepEnemyHealthBars(s, seen.data(), 1, 0.02f);
	seen[0] = Fighter(0xB, 0.7f, 4.0f, false);
	StepEnemyHealthBars(s, seen.data(), 1, 1.5f);
	Check(Near(s.bar[0].linger, 1.5f), "stopped fighting: lingering");
	seen[0] = Fighter(0xB, 0.6f, 4.0f, true);
	v = StepEnemyHealthBars(s, seen.data(), 1, 0.02f);
	Check(v.shown == 1 && Near(s.bar[0].linger, 0.0f) && Near(s.bar[0].fraction, 0.6f),
	      "fighting again: the linger reset, the health taken");

	// A dead one never seen before: no bar.
	s = EnemyHealthState{};
	seen[0] = Fighter(0xC, 0.0f, 2.0f, false, true);
	v = StepEnemyHealthBars(s, seen.data(), 1, 0.02f);
	Check(v.started == 0 && v.shown == 0 && s.bar[0].actor == 0, "a corpse: no bar begun");
	seen[0] = Fighter(0xC, 0.5f, 2.0f, false, false);
	v = StepEnemyHealthBars(s, seen.data(), 1, 0.02f);
	Check(v.started == 0 && s.bar[0].actor == 0, "one alive but not fighting: no bar begun");
	seen[0] = Fighter(0, 0.5f, 2.0f);
	v = StepEnemyHealthBars(s, seen.data(), 1, 0.02f);
	Check(v.started == 0, "no actor: no bar");

	// No time: nothing lingers out, nothing counts.
	s = EnemyHealthState{};
	seen[0] = Fighter(0xB, 0.7f, 4.0f);
	StepEnemyHealthBars(s, seen.data(), 1, 0.0f);
	v = StepEnemyHealthBars(s, seen.data(), 0, -1.0f);
	Check(v.shown == 1 && Near(s.bar[0].linger, 0.0f) && Near(s.bar[0].seconds, 0.0f), "a negative or zero dt: as 0");
}

void TestSlots() {
	std::printf("The slots\n");
	EnemyHealthState s;
	std::vector<EnemyHealthSeen> seen;
	for (UInt32 i = 0; i < 10; ++i) {
		// The farthest first in the list, so the order has to be by distance.
		seen.push_back(Fighter(0x100 + i, 1.0f, 20.0f - static_cast<float>(i)));
	}
	EnemyHealthVerdict v = StepEnemyHealthBars(s, seen.data(), 10, 0.02f);
	Check(v.started == kEnemyHealthBarsMax && v.shown == kEnemyHealthBarsMax, "ten fighters: eight bars");
	Check(s.bar[0].actor == 0x109 && s.bar[1].actor == 0x108 && s.bar[7].actor == 0x102,
	      "the nearest eight, nearest first");
	Check(EnemyHealthBarOf(s, 0x100) < 0 && EnemyHealthBarOf(s, 0x101) < 0, "the two farthest have none");
	// One of the eight goes: the next frame a free slot takes the nearest
	// of the rest, once the gone one's linger is over.
	seen.erase(seen.begin() + 9);  // 0x109 gone from the list
	v = StepEnemyHealthBars(s, seen.data(), 9, 0.02f);
	Check(v.started == 0 && v.shown == kEnemyHealthBarsMax && s.bar[0].actor == 0x109, "gone: its bar lingers, no slot free");
	v = StepEnemyHealthBars(s, seen.data(), 9, kEnemyHealthLingerSeconds);
	Check(v.ended == 1 && v.started == 1 && s.bar[0].actor == 0x101,
	      "the linger over: the slot freed and given to the nearest without a bar (0x101)");
	Check(v.shown == kEnemyHealthBarsMax, "eight shown again");
}

void TestPainter() {
	std::printf("The painter\n");
	const SInt32 inner = static_cast<SInt32>(obvr::ui::kEnemyHealthCanvasWidth) - 2 * obvr::ui::kEnemyHealthInset;
	Check(obvr::ui::EnemyHealthFillPixels(1.0f, inner) == inner, "full: the whole inner width");
	Check(obvr::ui::EnemyHealthFillPixels(0.5f, inner) == inner / 2, "half: half of it");
	Check(obvr::ui::EnemyHealthFillPixels(0.0f, inner) == 0, "empty: nothing");
	Check(obvr::ui::EnemyHealthFillPixels(0.001f, inner) == 1, "a sliver left: one pixel");
	Check(obvr::ui::EnemyHealthFillPixels(1.5f, inner) == inner, "over full: the whole width");
	Check(obvr::ui::EnemyHealthFillPixels(kNaN, inner) == 0, "not a number: nothing");
	Check(obvr::ui::EnemyHealthFillPixels(1.0f, 0) == 0, "no inner width: nothing");

	std::vector<obvr::render::Pixel> pixels(obvr::ui::kEnemyHealthCanvasWidth * obvr::ui::kEnemyHealthCanvasHeight);
	obvr::ui::Canvas canvas(pixels.data(), obvr::ui::kEnemyHealthCanvasWidth, obvr::ui::kEnemyHealthCanvasHeight);
	obvr::ui::EnemyHealthView view;
	view.fraction = 0.5f;
	obvr::ui::PaintEnemyHealthFor(canvas, &view);
	const SInt32 y = static_cast<SInt32>(obvr::ui::kEnemyHealthCanvasHeight) / 2;
	const obvr::render::Pixel left = canvas.GetPixel(obvr::ui::kEnemyHealthInset + 2, y);
	const obvr::render::Pixel right = canvas.GetPixel(static_cast<SInt32>(obvr::ui::kEnemyHealthCanvasWidth) - obvr::ui::kEnemyHealthInset - 3, y);
	const obvr::render::Pixel frame = canvas.GetPixel(0, 0);
	// Painted with red and blue swapped (the texture's order): the fill's
	// red is in .b, the bed's and the frame's in .b too.
	Check(left.b > 150 && left.g < 60, "at half: the left of the inner width is the fill (red)");
	Check(right.b < 60 && right.a > 100, "the right of it is the bed (dark)");
	Check(frame.b > 200 && frame.g > 150 && frame.r > 100, "the corner is the gold frame");
	Check(left.a > 200, "the fill is nearly opaque");
	view.fraction = 0.0f;
	obvr::ui::PaintEnemyHealth(canvas, view);
	const obvr::render::Pixel empty = canvas.GetPixel(obvr::ui::kEnemyHealthInset + 2, y);
	Check(empty.b < 60, "empty: no fill at the left");
}

}  // namespace

// The game's own frames: the frame for a fraction, the DXT5 decoding, the
// frame scaled onto the canvas.
void TestFrames() {
	std::printf("The game's own frames\n");
	Check(EnemyHealthFrameIndex(1.0f, 40) == 0 && EnemyHealthFrameIndex(0.0f, 40) == 39, "full the first frame, empty the last");
	Check(EnemyHealthFrameIndex(0.5f, 40) == 20 && EnemyHealthFrameIndex(0.75f, 40) == 10 &&
	          EnemyHealthFrameIndex(0.99f, 40) == 0 && EnemyHealthFrameIndex(0.97f, 40) == 1,
	      "in between the nearest frame");
	Check(EnemyHealthFrameIndex(1.5f, 40) == 0 && EnemyHealthFrameIndex(-1.0f, 40) == 39 &&
	          EnemyHealthFrameIndex(kNaN, 40) == 0,
	      "out of range clamped, not a number full");
	Check(EnemyHealthFrameIndex(0.3f, 1) == 0 && EnemyHealthFrameIndex(0.3f, 0) == 0, "fewer than two frames: the first");

	// A DXT5 block: alphas 255 and 0 (the eight-step table), the colours
	// pure red (0xF800) and pure blue (0x001F) with their two mixes; the
	// alpha indices all 0 but the last pixel's (7 = alpha 0 under a0 > a1 ...
	// the eighth entry is a1 itself here: 255 -> (7-6)*255/7 = 36 at index 7);
	// the colour indices 0, 1, 2, 3 across the first row, then 0.
	UInt8 block[16] = {};
	block[0] = 255;
	block[1] = 0;
	// alpha index 7 for pixel 15: bits 45..47 of the 48-bit field -> byte 7 (bits 40..47), its top three bits.
	block[7] = 0xE0;
	block[8] = 0x00;
	block[9] = 0xF8;  // c0 = 0xF800 red
	block[10] = 0x1F;
	block[11] = 0x00;  // c1 = 0x001F blue
	block[12] = 0xE4;  // pixels 0..3: indices 0, 1, 2, 3 (binary 11 10 01 00)
	obvr::render::Pixel px[16];
	obvr::ui::DecodeDxt5Block(block, px);
	// The canvas's order: red in .b, blue in .r.
	Check(px[0].b == 255 && px[0].g == 0 && px[0].r == 0 && px[0].a == 255, "pixel 0: red (in the canvas's order), opaque");
	Check(px[1].r == 255 && px[1].b == 0 && px[1].a == 255, "pixel 1: blue");
	Check(px[2].b == 170 && px[2].r == 85 && px[2].g == 0, "pixel 2: two thirds red, a third blue");
	Check(px[3].b == 85 && px[3].r == 170, "pixel 3: a third red, two thirds blue");
	Check(px[4].b == 255 && px[4].a == 255, "pixel 4: red again (index 0)");
	Check(px[15].a == 36, "pixel 15: the alpha table's last entry under a0 > a1");
	// a0 <= a1: the six-step table with 0 and 255 at the end.
	block[0] = 0;
	block[1] = 100;
	block[7] = 0xE0;  // pixel 15 index 7 -> 255
	block[6] = 0x18;  // pixel 14: bits 42..44 -> index 6 -> 0 ... (0x18 = bits 43,44 of byte 6 = bits 43,44 -> index 6 = 110b)
	obvr::ui::DecodeDxt5Block(block, px);
	Check(px[15].a == 255 && px[0].a == 0, "a0 <= a1: index 7 is 255, index 0 the first endpoint 0");
	Check(px[14].a == 0, "a0 <= a1: index 6 is 0");
	// A whole picture: 8 by 4 of one block twice, the pitch 16.
	UInt8 picture[32];
	for (int i = 0; i < 16; ++i) {
		picture[i] = block[i];
		picture[16 + i] = block[i];
	}
	std::vector<obvr::render::Pixel> out(8 * 4);
	Check(obvr::ui::DecodeDxt5(picture, 8, 4, 32, out.data()), "an 8 by 4 picture decodes");
	Check(out[0].b == 255 && out[4].b == 255 && out[5].r == 255, "both blocks in place");
	Check(!obvr::ui::DecodeDxt5(picture, 6, 4, 32, out.data()) && !obvr::ui::DecodeDxt5(nullptr, 8, 4, 32, out.data()),
	      "a width not a multiple of 4, or nothing: refused");
	const UInt8 argb[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	obvr::render::Pixel two[2];
	Check(obvr::ui::CopyA8R8G8B8(argb, 2, 1, 8, two) && two[0].r == 1 && two[0].a == 4 && two[1].r == 5,
	      "A8R8G8B8 copied byte for byte");

	// The frame scaled onto the canvas: a 2 by 1 frame, red then blue, on a
	// 256 by 64 canvas: the left half red, the right half blue, alpha kept.
	Check(obvr::ui::FrameSource(0, 256, 2) == 0 && obvr::ui::FrameSource(127, 256, 2) == 0 &&
	          obvr::ui::FrameSource(128, 256, 2) == 1 && obvr::ui::FrameSource(255, 256, 2) == 1,
	      "the nearest source column");
	Check(obvr::ui::FrameSource(10, 0, 2) == 0 && obvr::ui::FrameSource(10, 256, 0) == 0, "no size: the first");
	obvr::render::Pixel frame[2] = {obvr::ui::SwapRedAndBlue(obvr::render::Pixel{255, 0, 0, 200}),
	                                obvr::ui::SwapRedAndBlue(obvr::render::Pixel{0, 0, 255, 100})};
	std::vector<obvr::render::Pixel> pixels(obvr::ui::kEnemyHealthCanvasWidth * obvr::ui::kEnemyHealthCanvasHeight);
	obvr::ui::Canvas canvas(pixels.data(), obvr::ui::kEnemyHealthCanvasWidth, obvr::ui::kEnemyHealthCanvasHeight);
	obvr::ui::EnemyHealthView view;
	view.fraction = 0.1f;
	view.frame = frame;
	view.frameWidth = 2;
	view.frameHeight = 1;
	obvr::ui::PaintEnemyHealthFor(canvas, &view);
	const obvr::render::Pixel l = canvas.GetPixel(10, 30);
	const obvr::render::Pixel r = canvas.GetPixel(250, 30);
	Check(l.b == 255 && l.r == 0 && l.a == 200, "the left of the canvas the frame's red, its alpha kept");
	Check(r.r == 255 && r.b == 0 && r.a == 100, "the right the frame's blue");
	view.frame = nullptr;
	obvr::ui::PaintEnemyHealthFor(canvas, &view);
	Check(canvas.GetPixel(0, 0).g > 150, "without a frame OBVR's own bar (the gold corner)");
}

// The game's memory as the lookup sees it: a few dwords at known addresses,
// every read recorded. A read of any other address is one the lookup must
// not make - under Proton such an address need not be mapped.
struct FakeMemory {
	struct Cell {
		UInt32 address;
		UInt32 value;
	};
	std::vector<Cell> cells;
	std::vector<UInt32> reads;
	bool strayRead = false;

	void Set(UInt32 address, UInt32 value) {
		for (Cell& c : cells) {
			if (c.address == address) {
				c.value = value;
				return;
			}
		}
		cells.push_back({address, value});
	}
	UInt32 Read(UInt32 address) {
		reads.push_back(address);
		for (const Cell& c : cells) {
			if (c.address == address) {
				return c.value;
			}
		}
		strayRead = true;
		return 0;
	}
	bool WasRead(UInt32 address) const {
		for (UInt32 a : reads) {
			if (a == address) {
				return true;
			}
		}
		return false;
	}
};

void TestTextureLookup() {
	std::printf("The frame's Direct3D texture\n");
	// The values of the crash of 2026-10-14 where it had them: the renderer
	// data in ebx, IDirect3DTexture9's vtable in esi, "DXT5" at +0x18.
	constexpr UInt32 kTexture = 0x18D3A000;  // a NiSourceTexture
	constexpr UInt32 kData = 0x1832EC10;     // its renderer data
	constexpr UInt32 kD3D = 0x2A10F0A0;      // the IDirect3DTexture9
	constexpr UInt32 kVtable = 0x7925E258;   // IDirect3DTexture9's vtable
	constexpr UInt32 kDxt5 = 0x35545844;     // the pixel format's D3DFORMAT, "DXT5"
	auto game = [&](FakeMemory& m) {
		m.Set(kTexture + 0x24, kData);
		m.Set(kData + 0x18, kDxt5);
		m.Set(kData + 0x50, kD3D);
		m.Set(kD3D, kVtable);
	};

	{
		FakeMemory m;
		game(m);
		const D3DTextureLookup r = LookUpD3DTexture(kTexture, kVtable, [&m](UInt32 a) { return m.Read(a); });
		Check(r.result == D3DTextureResult::Found && r.texture == kD3D && r.rendererData == kData,
		      "found: the renderer data's +0x50, told by its vtable");
		Check(m.reads.size() == 3 && m.WasRead(kTexture + 0x24) && m.WasRead(kData + 0x50) && m.WasRead(kD3D) &&
		          !m.strayRead,
		      "three reads: the renderer data, its +0x50, that object's vtable");
		Check(!m.WasRead(kData + 0x18) && !m.WasRead(kDxt5),
		      "the format at +0x18 neither read nor followed (the crash of 2026-10-14)");
	}
	{
		FakeMemory m;
		game(m);
		const D3DTextureLookup r = LookUpD3DTexture(kTexture, 0, [&m](UInt32 a) { return m.Read(a); });
		Check(r.result == D3DTextureResult::NoVtable && r.texture == 0 && m.reads.empty(),
		      "no vtable to tell the texture by: nothing read");
	}
	{
		FakeMemory m;
		game(m);
		const D3DTextureLookup r = LookUpD3DTexture(0x00000080, kVtable, [&m](UInt32 a) { return m.Read(a); });
		Check(r.result == D3DTextureResult::NoRendererData && r.texture == 0 && m.reads.empty(),
		      "no texture object: nothing read");
	}
	{
		FakeMemory m;
		game(m);
		m.Set(kTexture + 0x24, 0);
		const D3DTextureLookup r = LookUpD3DTexture(kTexture, kVtable, [&m](UInt32 a) { return m.Read(a); });
		Check(r.result == D3DTextureResult::NoRendererData && r.texture == 0 && m.reads.size() == 1 && !m.strayRead,
		      "no renderer data: one read, nothing followed");
	}
	{
		FakeMemory m;
		game(m);
		m.Set(kData + 0x50, 0);
		const D3DTextureLookup r = LookUpD3DTexture(kTexture, kVtable, [&m](UInt32 a) { return m.Read(a); });
		Check(r.result == D3DTextureResult::NoTexture && r.texture == 0 && r.rendererData == kData &&
		          m.reads.size() == 2 && !m.strayRead,
		      "no Direct3D texture yet at +0x50: two reads, the renderer data kept for the probe");
	}
	{
		FakeMemory m;
		game(m);
		m.Set(kD3D, kVtable + 0x100);
		const D3DTextureLookup r = LookUpD3DTexture(kTexture, kVtable, [&m](UInt32 a) { return m.Read(a); });
		Check(r.result == D3DTextureResult::OtherObject && r.texture == 0 && m.reads.size() == 3 && !m.strayRead,
		      "another object at +0x50 (a cube texture's vtable): refused after its vtable");
	}

	std::printf("The probe's line\n");
	const UInt32 dwords[4] = {0x00A883C4, kTexture, 0x00000080, kDxt5};
	char line[128];
	Check(FormatProbeDwords(line, sizeof(line), "renderer data", kData, dwords, 4) == 4 &&
	          std::strcmp(line, "renderer data 1832EC10: +00=00A883C4 +04=18D3A000 +08=00000080 +0C=35545844") == 0,
	      "the dwords as they are, none followed");
	char shortLine[40];
	Check(FormatProbeDwords(shortLine, sizeof(shortLine), "renderer data", kData, dwords, 4) == 1 &&
	          std::strcmp(shortLine, "renderer data 1832EC10: +00=00A883C4") == 0,
	      "a short buffer: the dwords that fit whole, terminated");
	char tiny[8];
	Check(FormatProbeDwords(tiny, sizeof(tiny), "renderer data", kData, dwords, 4) == 0 && std::strlen(tiny) < sizeof(tiny),
	      "no room for the label: nothing but the cut label, terminated");
	Check(FormatProbeDwords(nullptr, 0, "renderer data", kData, dwords, 4) == 0, "no buffer: nothing");
}

int main() {
	TestFrames();
	TestRules();
	TestBarFlows();
	TestSlots();
	TestPainter();
	TestTextureLookup();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
