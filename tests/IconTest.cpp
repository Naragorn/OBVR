// Checks the quick menu's item icons: the DDS reader (render/DdsImage.h) on
// hand-made blocks of every format it takes and every file it refuses, the
// fit into the icon square, the icon's file path and cache place
// (game/ItemIcons.h), and laying a picture over the canvas (ui/MenuCanvas.h).

#include <cstdio>
#include <cstring>
#include <vector>

#include "game/ItemIcons.h"
#include "render/DdsImage.h"
#include "ui/MenuCanvas.h"

using namespace obvr;
using render::Pixel;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Same(Pixel a, Pixel b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

void Put32(std::vector<UInt8>& f, UInt32 at, UInt32 v) {
	f[at] = static_cast<UInt8>(v);
	f[at + 1] = static_cast<UInt8>(v >> 8);
	f[at + 2] = static_cast<UInt8>(v >> 16);
	f[at + 3] = static_cast<UInt8>(v >> 24);
}

// A header for a width x height DDS: a four-character code, or (fourCc 0)
// bit masks.
std::vector<UInt8> Header(UInt32 width, UInt32 height, const char* fourCc, UInt32 bits = 0,
                          UInt32 r = 0, UInt32 g = 0, UInt32 b = 0, UInt32 a = 0) {
	std::vector<UInt8> f(render::kDdsHeaderBytes, 0);
	std::memcpy(f.data(), "DDS ", 4);
	Put32(f, 4, 124);
	Put32(f, 12, height);
	Put32(f, 16, width);
	Put32(f, 76, 32);
	if (fourCc != nullptr) {
		Put32(f, 80, 0x4);
		std::memcpy(f.data() + 84, fourCc, 4);
	} else {
		Put32(f, 80, a != 0 ? 0x41u : 0x40u);
		Put32(f, 88, bits);
		Put32(f, 92, r);
		Put32(f, 96, g);
		Put32(f, 100, b);
		Put32(f, 104, a);
	}
	return f;
}

void Append(std::vector<UInt8>& f, std::initializer_list<UInt8> bytes) {
	f.insert(f.end(), bytes.begin(), bytes.end());
}

// A colour block: c0, c1 (5:6:5) and the sixteen 2-bit indices, pixel 0 in
// the lowest bits.
void ColourBlock(std::vector<UInt8>& f, UInt16 c0, UInt16 c1, UInt32 indices) {
	Append(f, {static_cast<UInt8>(c0), static_cast<UInt8>(c0 >> 8), static_cast<UInt8>(c1),
	           static_cast<UInt8>(c1 >> 8), static_cast<UInt8>(indices), static_cast<UInt8>(indices >> 8),
	           static_cast<UInt8>(indices >> 16), static_cast<UInt8>(indices >> 24)});
}

constexpr UInt16 kRed565 = 0xF800;
constexpr UInt16 kBlue565 = 0x001F;

void TestDxt1() {
	std::printf("DXT1\n");
	// Four-colour mode (red > blue as numbers): indices 0,1,2,3 in the first
	// row, then all 0.
	std::vector<UInt8> f = Header(4, 4, "DXT1");
	ColourBlock(f, kRed565, kBlue565, 0x000000E4);
	std::vector<Pixel> px;
	UInt32 w = 0;
	UInt32 h = 0;
	Check(render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h) && w == 4 && h == 4,
	      "a 4x4 DXT1 decodes");
	Check(Same(px[0], Pixel{255, 0, 0, 255}) && Same(px[1], Pixel{0, 0, 255, 255}), "the end colours");
	Check(Same(px[2], Pixel{170, 0, 85, 255}) && Same(px[3], Pixel{85, 0, 170, 255}),
	      "four colours: two thirds and one third between");
	Check(Same(px[4], Pixel{255, 0, 0, 255}), "the next row: index 0");

	// Three-colour mode (the first end colour not the greater): 2 is half
	// way, 3 transparent.
	f = Header(4, 4, "DXT1");
	ColourBlock(f, kBlue565, kRed565, 0x000000E4);
	render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h);
	Check(Same(px[2], Pixel{127, 0, 127, 255}), "three colours: half way");
	Check(px[3].a == 0, "and the fourth transparent");

	// A 6x5 picture: two by two blocks, the pixels past the edge dropped.
	f = Header(6, 5, "DXT1");
	for (int i = 0; i < 4; ++i) {
		ColourBlock(f, i == 3 ? kBlue565 : kRed565, 0, 0);
	}
	Check(render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h) && px.size() == 30,
	      "a size not a multiple of four: whole blocks read, the picture as big as it says");
	Check(Same(px[4 * 6 + 5], Pixel{0, 0, 255, 255}), "its last pixel from the last block");
}

void TestDxt3() {
	std::printf("DXT3\n");
	std::vector<UInt8> f = Header(4, 4, "DXT3");
	// Alpha: pixel 0 = 0, pixel 1 = 15, pixel 2 = 8, the rest 15.
	Append(f, {0xF0, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
	// The colour block in three-colour order is still read with four.
	ColourBlock(f, kBlue565, kRed565, 0x000000C0);
	std::vector<Pixel> px;
	UInt32 w = 0;
	UInt32 h = 0;
	Check(render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "a 4x4 DXT3 decodes");
	Check(px[0].a == 0 && px[1].a == 255 && px[2].a == 136, "four bits of alpha each, widened");
	Check(Same(px[3], Pixel{170, 0, 85, 255}), "its colours always four: index 3 is not transparent");
}

void TestDxt5() {
	std::printf("DXT5\n");
	std::vector<UInt8> f = Header(4, 4, "DXT5");
	// a0 200 > a1 100: eight values. Indices: pixel 0 -> 0, pixel 1 -> 1,
	// pixel 2 -> 2, pixel 9 -> 7, the rest 0.
	// Bits: p1 at 3..5 = 1, p2 at 6..8 = 2; p9 at 27..29 = 7 (the high half,
	// bits 3..5 of it).
	const UInt32 low = (1u << 3) | (2u << 6);
	const UInt32 high = 7u << 3;
	Append(f, {200, 100, static_cast<UInt8>(low), static_cast<UInt8>(low >> 8), static_cast<UInt8>(low >> 16),
	           static_cast<UInt8>(high), static_cast<UInt8>(high >> 8), static_cast<UInt8>(high >> 16)});
	ColourBlock(f, kRed565, kBlue565, 0);
	std::vector<Pixel> px;
	UInt32 w = 0;
	UInt32 h = 0;
	Check(render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "a 4x4 DXT5 decodes");
	Check(px[0].a == 200 && px[1].a == 100, "the end alphas");
	Check(px[2].a == (6 * 200 + 1 * 100) / 7, "eight values: sevenths between");
	Check(px[9].a == (1 * 200 + 6 * 100) / 7, "an index in the second half of the bits");

	// a0 <= a1: six values, 6 is clear and 7 opaque.
	f = Header(4, 4, "DXT5");
	const UInt32 low2 = (6u << 0) | (7u << 3) | (2u << 6);
	Append(f, {50, 150, static_cast<UInt8>(low2), static_cast<UInt8>(low2 >> 8), static_cast<UInt8>(low2 >> 16),
	           0, 0, 0});
	ColourBlock(f, kRed565, kBlue565, 0);
	render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h);
	Check(px[0].a == 0 && px[1].a == 255, "six values: 6 clear, 7 opaque");
	Check(px[2].a == (4 * 50 + 1 * 150) / 5, "and fifths between");
}

void TestMasked() {
	std::printf("Uncompressed\n");
	// A8R8G8B8: bytes blue, green, red, alpha.
	std::vector<UInt8> f = Header(2, 1, nullptr, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	Append(f, {10, 20, 30, 40, 50, 60, 70, 80});
	std::vector<Pixel> px;
	UInt32 w = 0;
	UInt32 h = 0;
	Check(render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h) && w == 2 && h == 1,
	      "32 bits a pixel decodes");
	Check(Same(px[0], Pixel{30, 20, 10, 40}) && Same(px[1], Pixel{70, 60, 50, 80}), "each channel by its mask");

	// A4R4G4B4.
	f = Header(1, 1, nullptr, 16, 0x0F00, 0x00F0, 0x000F, 0xF000);
	Append(f, {0x3F, 0x8C});  // 0x8C3F: a 8, r C, g 3, b F
	render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h);
	Check(Same(px[0], Pixel{204, 51, 255, 136}), "16 bits with four of alpha, widened");

	// R8G8B8, no alpha mask: opaque.
	f = Header(1, 1, nullptr, 24, 0xFF0000, 0x00FF00, 0x0000FF, 0);
	Append(f, {1, 2, 3});
	render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h);
	Check(Same(px[0], Pixel{3, 2, 1, 255}), "24 bits without alpha: opaque");
}

void TestRefused() {
	std::printf("Refused\n");
	std::vector<Pixel> px;
	UInt32 w = 7;
	UInt32 h = 7;
	std::vector<UInt8> f = Header(4, 4, "DXT1");
	ColourBlock(f, kRed565, kBlue565, 0);
	f.pop_back();
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h) && w == 0 && h == 0,
	      "a file shorter than its top level");
	f = Header(4, 4, "DX10");
	f.resize(f.size() + 64);
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "a DX10 header");
	f = Header(4, 4, "DXT2");
	f.resize(f.size() + 64);
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "DXT2");
	f = Header(4, 4, "DXT1");
	f[0] = 'X';
	f.resize(f.size() + 64);
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "not \"DDS \"");
	f = Header(4, 4, "DXT1");
	Put32(f, 4, 100);
	f.resize(f.size() + 64);
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "a header of the wrong size");
	f = Header(0, 4, "DXT1");
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "no width");
	f = Header(4096, 4, "DXT1");
	f.resize(f.size() + 4096 * 2);
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "wider than an icon can be");
	f = Header(1, 1, nullptr, 8, 0xE0, 0x1C, 0x03, 0);
	f.resize(f.size() + 4);
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "8 bits a pixel");
	f = Header(1, 1, nullptr, 32, 0, 0, 0, 0xFF000000);
	f.resize(f.size() + 4);
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "no colour masks");
	f = Header(1, 1, nullptr, 32, 0xFF, 0, 0, 0);
	Put32(f, 80, 0x20000);  // luminance
	f.resize(f.size() + 4);
	Check(!render::DecodeDds(f.data(), static_cast<UInt32>(f.size()), px, w, h), "luminance");
	Check(!render::DecodeDds(nullptr, 500, px, w, h), "no data");
	render::DdsInfo info;
	f = Header(64, 64, "DXT3");
	f.resize(f.size() + 16 * 16 * 16);
	Check(render::ReadDdsHeader(f.data(), static_cast<UInt32>(f.size()), info) &&
	          info.format == render::DdsInfo::Format::Dxt3 && info.width == 64,
	      "a vanilla icon's header (64x64 DXT3, 4224 bytes) reads");
}

void TestSurface() {
	std::printf("A locked texture level\n");
	std::vector<Pixel> px;
	// A8R8G8B8, 2x2, rows padded to 16 bytes.
	UInt8 argb[32] = {};
	const UInt8 row0[8] = {10, 20, 30, 255, 50, 60, 70, 255};
	const UInt8 row1[8] = {1, 2, 3, 255, 5, 6, 7, 0};
	std::memcpy(argb, row0, 8);
	std::memcpy(argb + 16, row1, 8);
	Check(render::DecodeSurface(render::kD3dFormatA8R8G8B8, argb, 16, 2, 2, px) && px.size() == 4,
	      "A8R8G8B8 with a padded pitch");
	Check(Same(px[0], Pixel{30, 20, 10, 255}) && Same(px[2], Pixel{3, 2, 1, 255}) && px[3].a == 0,
	      "each row from its pitch");
	Check(!render::DecodeSurface(render::kD3dFormatA8R8G8B8, argb, 4, 2, 2, px), "a pitch shorter than a row: refused");
	// DXT1, a 1x1 level: one block, only its first pixel.
	std::vector<UInt8> block;
	ColourBlock(block, kRed565, kBlue565, 0);
	Check(render::DecodeSurface(render::kD3dFormatDxt1, block.data(), 8, 1, 1, px) && px.size() == 1 &&
	          Same(px[0], Pixel{255, 0, 0, 255}),
	      "a DXT1 level smaller than its block");
	std::vector<UInt8> two;
	Append(two, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});
	ColourBlock(two, kBlue565, kRed565, 0);
	Check(render::DecodeSurface(render::kD3dFormatDxt3, two.data(), 16, 4, 4, px) && px[0].b == 255,
	      "DXT3 by its four-character code");
	Check(render::DecodeSurface(render::kD3dFormatDxt5, two.data(), 16, 2, 2, px), "DXT5 by its code");
	UInt8 x8[4] = {9, 8, 7, 0};
	Check(render::DecodeSurface(render::kD3dFormatX8R8G8B8, x8, 4, 1, 1, px) && Same(px[0], Pixel{7, 8, 9, 255}),
	      "X8R8G8B8: its unused byte is not alpha");
	UInt8 r8[3] = {9, 8, 7};
	Check(render::DecodeSurface(render::kD3dFormatR8G8B8, r8, 3, 1, 1, px) && Same(px[0], Pixel{7, 8, 9, 255}),
	      "R8G8B8");
	UInt8 w565[2] = {0x00, 0xF8};
	Check(render::DecodeSurface(render::kD3dFormatR5G6B5, w565, 2, 1, 1, px) && Same(px[0], Pixel{255, 0, 0, 255}),
	      "R5G6B5");
	UInt8 w1555[2] = {0x1F, 0x80};
	Check(render::DecodeSurface(render::kD3dFormatA1R5G5B5, w1555, 2, 1, 1, px) && Same(px[0], Pixel{0, 0, 255, 255}),
	      "A1R5G5B5");
	UInt8 w4444[2] = {0x0F, 0x00};
	Check(render::DecodeSurface(render::kD3dFormatA4R4G4B4, w4444, 2, 1, 1, px) && Same(px[0], Pixel{0, 0, 255, 0}),
	      "A4R4G4B4");
	Check(!render::DecodeSurface(50, x8, 4, 1, 1, px), "a format not read (L8): refused");
	Check(!render::DecodeSurface(render::kD3dFormatA8R8G8B8, nullptr, 4, 1, 1, px), "no bits: refused");
	Check(!render::DecodeSurface(render::kD3dFormatA8R8G8B8, x8, 4, 0, 1, px), "no width: refused");
	Check(!render::DecodeSurface(render::kD3dFormatA8R8G8B8, x8, 4, 2000, 1, px), "wider than read: refused");

	const Pixel mix[2] = {{200, 100, 0, 10}, {100, 50, 255, 255}};
	Check(Same(render::AverageColour(mix, 2), Pixel{150, 75, 127, 255}), "the plain average, opaque");
	Check(render::AverageColour(nullptr, 3).a == 0 && render::AverageColour(mix, 0).a == 0, "none: clear");
}

void TestFit() {
	std::printf("Fitting into the square\n");
	Pixel dst[4 * 4];
	// 2x2 grown to 4x4: each pixel twice each way.
	const Pixel small[4] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}, {9, 9, 9, 255}};
	render::FitSquare(small, 2, 2, dst, 4);
	Check(Same(dst[0], small[0]) && Same(dst[1], small[0]) && Same(dst[2], small[1]) && Same(dst[15], small[3]),
	      "smaller: each pixel repeated");
	// 4x4 shrunk to 2x2: each the average of four, weighted by alpha.
	Pixel big[16];
	for (Pixel& p : big) {
		p = Pixel{200, 100, 0, 255};
	}
	big[0] = Pixel{0, 0, 0, 0};  // a clear pixel's colour must not darken the edge
	Pixel two[4];
	render::FitSquare(big, 4, 4, two, 2);
	Check(Same(two[0], Pixel{200, 100, 0, 191}) && Same(two[3], Pixel{200, 100, 0, 255}),
	      "bigger: averaged, a clear pixel thinning the alpha, not the colour");
	for (Pixel& p : big) {
		p = Pixel{1, 2, 3, 0};
	}
	render::FitSquare(big, 4, 4, two, 2);
	Check(two[0].a == 0, "all clear stays clear");
	// 4x2 in a 4x4 square: centred, above and below clear.
	Pixel wide[8];
	for (Pixel& p : wide) {
		p = Pixel{50, 60, 70, 255};
	}
	render::FitSquare(wide, 4, 2, dst, 4);
	Check(dst[0].a == 0 && Same(dst[4], wide[0]) && Same(dst[11], wide[0]) && dst[12].a == 0,
	      "not square: its shape kept, centred");
	render::FitSquare(nullptr, 4, 4, dst, 4);
	Check(dst[5].a == 0, "no picture: the square clear");
}

void TestPath() {
	std::printf("The icon's file\n");
	char out[64];
	Check(game::IconFilePath("Weapons\\IronLongsword.dds", out, sizeof(out)) &&
	          std::strcmp(out, "Textures\\Menus\\Icons\\Weapons\\IronLongsword.dds") == 0,
	      "under Textures\\Menus\\Icons\\, TESIcon's prefix");
	Check(game::IconFilePath("\\magic\\fire.dds", out, sizeof(out)) &&
	          std::strcmp(out, "Textures\\Menus\\Icons\\magic\\fire.dds") == 0,
	      "a leading slash dropped");
	Check(game::IconFilePath("textures\\menus\\icons\\x.dds", out, sizeof(out)) &&
	          std::strcmp(out, "textures\\menus\\icons\\x.dds") == 0,
	      "already from Textures\\: kept");
	Check(game::IconFilePath("Data/Textures/x.dds", out, sizeof(out)) && std::strcmp(out, "Data/Textures/x.dds") == 0,
	      "already from Data/: kept");
	Check(!game::IconFilePath("", out, sizeof(out)) && out[0] == '\0', "no path");
	Check(!game::IconFilePath("\\\\", out, sizeof(out)), "only slashes");
	Check(!game::IconFilePath(nullptr, out, sizeof(out)), "null");
	char tiny[16];
	Check(!game::IconFilePath("a.dds", tiny, sizeof(tiny)) && tiny[0] == '\0', "too long for the buffer: nothing");
	Check(!game::IconFilePath("a.dds", nullptr, 10), "no buffer");
}

void TestCache() {
	std::printf("The icon cache\n");
	bool used[game::kItemIconCacheSize] = {};
	UInt32 last[game::kItemIconCacheSize] = {};
	Check(game::IconCacheVictim(used, last) == 0, "empty: the first place");
	for (UInt32 i = 0; i < game::kItemIconCacheSize; ++i) {
		used[i] = i < 5;
		last[i] = 100 + i;
	}
	Check(game::IconCacheVictim(used, last) == 5, "the first unused place before any used one");
	for (UInt32 i = 0; i < game::kItemIconCacheSize; ++i) {
		used[i] = true;
	}
	last[7] = 3;
	Check(game::IconCacheVictim(used, last) == 7, "all used: the one asked for longest ago");
}

void TestBlend() {
	std::printf("Laying a picture over the canvas\n");
	const Pixel under{100, 100, 100, 200};
	Check(Same(ui::BlendOver(under, Pixel{1, 2, 3, 255}), Pixel{1, 2, 3, 255}), "opaque covers");
	Check(Same(ui::BlendOver(under, Pixel{1, 2, 3, 0}), under), "clear leaves it");
	const Pixel half = ui::BlendOver(Pixel{0, 0, 0, 255}, Pixel{255, 255, 255, 128});
	Check(half.r == 128 && half.a == 255, "half over opaque: half way, still opaque");
	const Pixel onClear = ui::BlendOver(Pixel{0, 0, 0, 0}, Pixel{200, 50, 10, 100});
	Check(Same(onClear, Pixel{200, 50, 10, 100}), "over clear: its own colour and alpha");

	std::vector<Pixel> buffer(4 * 4, Pixel{0, 0, 0, 0});
	ui::Canvas canvas(buffer.data(), 4, 4);
	const Pixel image[4] = {{9, 9, 9, 255}, {8, 8, 8, 255}, {7, 7, 7, 255}, {6, 6, 6, 0}};
	canvas.BlendImage(3, 3, image, 2, 2);
	Check(Same(canvas.GetPixel(3, 3), image[0]) && canvas.GetPixel(2, 3).a == 0, "clipped at the edge");
	canvas.BlendImage(0, 0, image, 2, 2);
	Check(Same(canvas.GetPixel(1, 1), Pixel{0, 0, 0, 0}) && Same(canvas.GetPixel(1, 0), image[1]),
	      "row by row, a clear pixel leaving the canvas as it was");
	canvas.BlendImage(0, 0, nullptr, 2, 2);
	Check(Same(canvas.GetPixel(0, 0), image[0]), "no picture: nothing drawn");
	canvas.BlendImage(-1, -1, image, 2, 2);
	Check(Same(canvas.GetPixel(0, 0), image[0]),
	      "half off the top left: only its last pixel lands, and that one is clear");
}

}  // namespace

int main() {
	TestDxt1();
	TestDxt3();
	TestDxt5();
	TestMasked();
	TestRefused();
	TestSurface();
	TestFit();
	TestPath();
	TestCache();
	TestBlend();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
