// Checks the dome's making (src/stage): the DDS and archive readers, the
// painter against the values of the preview Nadi chose (2026-11-15), the
// atlas and its mesh, the PNG writer read back, the kept dome's key, and a
// whole dome written from a Data folder made here - a texture's own file
// before the archive's.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "miniz.h"
#include "stage/BsaArchive.h"
#include "stage/DdsImage.h"
#include "stage/DomeAtlas.h"
#include "stage/DomeBuild.h"
#include "stage/DomeLook.h"
#include "stage/GameDomeKey.h"
#include "stage/PngStream.h"

namespace {

using namespace obvr::stage;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(double a, double b, double within) { return std::fabs(a - b) <= within; }

void PutU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
	for (int i = 0; i < 4; ++i) {
		out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
	}
}

// A DDS header; fourCc 0 for plain pixels of 'bits' bits, blue first.
std::vector<std::uint8_t> DdsHeader(std::uint32_t width, std::uint32_t height, std::uint32_t fourCc, std::uint32_t bits,
                                    bool alpha) {
	std::vector<std::uint8_t> d(128, 0);
	const auto put = [&d](std::size_t at, std::uint32_t v) {
		for (int i = 0; i < 4; ++i) {
			d[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
		}
	};
	put(0, 0x20534444u);
	put(4, 124);
	put(12, height);
	put(16, width);
	put(76, 32);
	if (fourCc != 0) {
		put(80, 0x4);
		put(84, fourCc);
	} else {
		put(80, 0x40u | (alpha ? 0x1u : 0u));
		put(88, bits);
		put(92, 0x00FF0000u);
		put(96, 0x0000FF00u);
		put(100, 0x000000FFu);
	}
	return d;
}

constexpr std::uint32_t kDxt1 = 0x31545844u;
constexpr std::uint32_t kDxt3 = 0x33545844u;
constexpr std::uint32_t kDxt5 = 0x35545844u;

// A 4 x 4 colour block: pure red and pure blue endpoints, every pixel the index given.
void ColourBlock(std::vector<std::uint8_t>& d, std::uint16_t c0, std::uint16_t c1, std::uint32_t indices) {
	d.push_back(static_cast<std::uint8_t>(c0));
	d.push_back(static_cast<std::uint8_t>(c0 >> 8));
	d.push_back(static_cast<std::uint8_t>(c1));
	d.push_back(static_cast<std::uint8_t>(c1 >> 8));
	PutU32(d, indices);
}

void TestDds() {
	std::printf("DDS\n");
	Image image;
	std::vector<std::uint8_t> dxt1 = DdsHeader(4, 4, kDxt1, 0, false);
	ColourBlock(dxt1, 0xF800, 0x001F, 0x00000001u | (2u << 2) | (3u << 4));  // red > blue: four colours
	Check(DecodeDds(dxt1.data(), dxt1.size(), image) && image.width == 4 && image.height == 4, "a DXT1 block decodes");
	Check(image.At(0, 0)[0] == 0 && image.At(0, 0)[2] == 255, "index 1 is the second endpoint, blue");
	Check(image.At(1, 0)[0] == 170 && image.At(1, 0)[2] == 85, "index 2 is two thirds of the first");
	Check(image.At(2, 0)[0] == 85 && image.At(2, 0)[2] == 170 && image.At(2, 0)[3] == 255, "index 3 is one third, opaque");
	Check(image.At(3, 0)[0] == 255 && image.At(3, 0)[2] == 0, "index 0 is the first endpoint, red");

	std::vector<std::uint8_t> punch = DdsHeader(4, 4, kDxt1, 0, false);
	ColourBlock(punch, 0x001F, 0xF800, 2u | (3u << 2));  // blue <= red: three colours and a clear one
	Check(DecodeDds(punch.data(), punch.size(), image) && image.At(0, 0)[0] == 127 && image.At(0, 0)[2] == 127,
	      "DXT1 with the smaller endpoint first: index 2 is the middle");
	Check(image.At(1, 0)[3] == 0 && image.At(1, 0)[0] == 0, "and index 3 is clear black");

	std::vector<std::uint8_t> dxt5 = DdsHeader(4, 4, kDxt5, 0, false);
	dxt5.push_back(200);
	dxt5.push_back(100);
	const std::uint8_t alphaBits[6] = {0x88, 0x00, 0x00, 0x00, 0x00, 0x00};  // pixel 0: index 0, pixel 1: index 1
	dxt5.insert(dxt5.end(), alphaBits, alphaBits + 6);
	ColourBlock(dxt5, 0x001F, 0xF800, 2u);  // smaller first, yet four colours: only DXT1 punches through
	Check(DecodeDds(dxt5.data(), dxt5.size(), image) && image.At(0, 0)[3] == 200 && image.At(1, 0)[3] == 100,
	      "DXT5: the alpha's two endpoints");
	Check(image.At(2, 0)[3] == 185 && image.At(0, 0)[0] == 85 && image.At(0, 0)[2] == 170,
	      "a step between them, and its colours always four");

	std::vector<std::uint8_t> dxt3 = DdsHeader(4, 4, kDxt3, 0, false);
	const std::uint8_t nibbles[8] = {0xF0, 0x08, 0, 0, 0, 0, 0, 0};
	dxt3.insert(dxt3.end(), nibbles, nibbles + 8);
	ColourBlock(dxt3, 0xF800, 0x001F, 0);
	Check(DecodeDds(dxt3.data(), dxt3.size(), image) && image.At(0, 0)[3] == 0 && image.At(1, 0)[3] == 255 &&
	          image.At(2, 0)[3] == 136,
	      "DXT3: four bits of alpha a pixel");

	std::vector<std::uint8_t> plain = DdsHeader(2, 1, 0, 32, true);
	const std::uint8_t two[8] = {10, 20, 30, 40, 50, 60, 70, 80};
	plain.insert(plain.end(), two, two + 8);
	Check(DecodeDds(plain.data(), plain.size(), image) && image.At(0, 0)[0] == 30 && image.At(0, 0)[2] == 10 &&
	          image.At(1, 0)[3] == 80,
	      "plain 32 bits, blue first, with its alpha");
	std::vector<std::uint8_t> rgb = DdsHeader(1, 1, 0, 24, false);
	rgb.push_back(1);
	rgb.push_back(2);
	rgb.push_back(3);
	Check(DecodeDds(rgb.data(), rgb.size(), image) && image.At(0, 0)[0] == 3 && image.At(0, 0)[3] == 255,
	      "plain 24 bits: opaque");

	Check(!DecodeDds(dxt1.data(), dxt1.size() - 1, image), "a block cut short is refused");
	Check(!DecodeDds(dxt1.data(), 100, image) && !DecodeDds(nullptr, 0, image), "no header: refused");
	std::vector<std::uint8_t> odd = DdsHeader(4, 4, 0x30315844u, 0, false);  // "DX10"
	odd.resize(odd.size() + 64);
	Check(!DecodeDds(odd.data(), odd.size(), image), "another format is refused");
	std::vector<std::uint8_t> huge = DdsHeader(16384, 4, kDxt1, 0, false);
	Check(!DecodeDds(huge.data(), huge.size(), image), "a side past 8192 is refused");
	std::vector<std::uint8_t> wrong = dxt1;
	wrong[0] = 'X';
	Check(!DecodeDds(wrong.data(), wrong.size(), image), "not a DDS: refused");
}

struct ArchiveFile {
	const char* folder;
	const char* name;
	std::vector<std::uint8_t> bytes;
	bool toggle;  // the size's bit that turns the archive's rule round
};

// A version 103 archive: each file its own folder record when the folder's
// name is new, the files stored as given.
std::vector<std::uint8_t> MakeArchive(const std::vector<ArchiveFile>& files, std::uint32_t flags) {
	std::vector<std::string> folders;
	for (const ArchiveFile& f : files) {
		if (folders.empty() || folders.back() != f.folder) {
			folders.push_back(f.folder);
		}
	}
	std::uint32_t folderNames = 0;
	std::uint32_t fileNames = 0;
	for (const std::string& f : folders) {
		folderNames += static_cast<std::uint32_t>(f.size()) + 1;
	}
	for (const ArchiveFile& f : files) {
		fileNames += static_cast<std::uint32_t>(std::strlen(f.name)) + 1;
	}
	const std::uint32_t folderCount = static_cast<std::uint32_t>(folders.size());
	const std::uint32_t fileCount = static_cast<std::uint32_t>(files.size());
	const std::uint32_t dataAt = 36 + folderCount * 16 + folderNames + folderCount + fileCount * 16 + fileNames;
	std::vector<std::uint8_t> a;
	PutU32(a, 0x00415342u);
	PutU32(a, 103);
	PutU32(a, 36);
	PutU32(a, flags);
	PutU32(a, folderCount);
	PutU32(a, fileCount);
	PutU32(a, folderNames);
	PutU32(a, fileNames);
	PutU32(a, 0);
	std::uint32_t blockAt = 36 + folderCount * 16;
	for (const std::string& folder : folders) {
		std::uint32_t count = 0;
		for (const ArchiveFile& f : files) {
			count += folder == f.folder ? 1 : 0;
		}
		PutU32(a, 0);
		PutU32(a, 0);
		PutU32(a, count);
		PutU32(a, blockAt + fileNames);
		blockAt += 1 + static_cast<std::uint32_t>(folder.size()) + 1 + count * 16;
	}
	std::uint32_t offset = dataAt;
	for (const std::string& folder : folders) {
		a.push_back(static_cast<std::uint8_t>(folder.size() + 1));
		a.insert(a.end(), folder.begin(), folder.end());
		a.push_back(0);
		for (const ArchiveFile& f : files) {
			if (folder == f.folder) {
				PutU32(a, 0);
				PutU32(a, 0);
				PutU32(a, static_cast<std::uint32_t>(f.bytes.size()) | (f.toggle ? 0x40000000u : 0u));
				PutU32(a, offset);
			}
			offset += folder == f.folder ? static_cast<std::uint32_t>(f.bytes.size()) : 0;
		}
	}
	for (const ArchiveFile& f : files) {
		a.insert(a.end(), f.name, f.name + std::strlen(f.name) + 1);
	}
	for (const std::string& folder : folders) {
		for (const ArchiveFile& f : files) {
			if (folder == f.folder) {
				a.insert(a.end(), f.bytes.begin(), f.bytes.end());
			}
		}
	}
	return a;
}

void TestArchive() {
	std::printf("Archive\n");
	const std::vector<ArchiveFile> files = {
		{"textures\\sky", "skystars.dds", {1, 2, 3}, false},
		{"textures\\sky", "masser_full.dds", {4, 5, 6, 7}, true},
		{"textures\\menus\\map\\world", "cyrodiil_resized.dds", {8, 9}, false},
	};
	const std::vector<std::uint8_t> a = MakeArchive(files, 0x7);
	const BsaHeader h = ReadBsaHeader(a.data(), a.size());
	Check(h.valid && h.folderCount == 2 && h.fileCount == 3, "the header reads");
	BsaEntry e;
	Check(FindInBsa(a.data(), a.size(), h, "textures\\sky\\skystars.dds", e) && e.size == 3 && a[e.offset] == 1 && e.compressed,
	      "the first file of a folder, compressed as the archive says");
	Check(FindInBsa(a.data(), a.size(), h, "Textures/Sky/MASSER_FULL.DDS", e) && e.size == 4 && a[e.offset] == 4 && !e.compressed,
	      "the second, any case and slash, its size bit turning the rule round");
	Check(FindInBsa(a.data(), a.size(), h, "textures\\menus\\map\\world\\cyrodiil_resized.dds", e) && a[e.offset] == 8,
	      "a file of the second folder: its name counted past the first folder's");
	Check(!FindInBsa(a.data(), a.size(), h, "textures\\sky\\secunda_full.dds", e), "a file that is not there");
	Check(!FindInBsa(a.data(), a.size(), h, "textures\\water\\skystars.dds", e), "a folder that is not there");
	Check(!FindInBsa(a.data(), a.size(), h, "skystars.dds", e), "a path without a folder");
	Check(!FindInBsa(a.data(), static_cast<std::size_t>(BsaDirectoryBytes(h)) - 1, h, "textures\\sky\\skystars.dds", e),
	      "a directory cut short is refused");
	const std::vector<std::uint8_t> stored = MakeArchive(files, 0x3);
	const BsaHeader hs = ReadBsaHeader(stored.data(), stored.size());
	Check(FindInBsa(stored.data(), stored.size(), hs, "textures\\sky\\skystars.dds", e) && !e.compressed,
	      "an archive of stored files");
	Check(FindInBsa(stored.data(), stored.size(), hs, "textures\\sky\\masser_full.dds", e) && e.compressed,
	      "and its one compressed file");
	std::vector<std::uint8_t> bad = a;
	bad[4] = 104;
	Check(!ReadBsaHeader(bad.data(), bad.size()).valid, "another version is refused");
	Check(!ReadBsaHeader(MakeArchive(files, 0x4).data(), 36).valid, "an archive without names is refused");
	Check(!ReadBsaHeader(a.data(), 35).valid && !ReadBsaHeader(nullptr, 0).valid, "a header cut short is refused");
	std::vector<std::uint8_t> broken = a;
	broken[36 + 12] = 0xFF;  // the first folder's offset past the directory
	broken[36 + 13] = 0xFF;
	Check(!FindInBsa(broken.data(), broken.size(), h, "textures\\sky\\skystars.dds", e), "a folder pointing outside is refused");
}

void TestSampling() {
	std::printf("Sampling\n");
	Image image;
	image.width = 2;
	image.height = 2;
	image.rgba = {0, 0, 0, 255, 100, 0, 0, 255, 0, 200, 0, 255, 100, 200, 0, 0};
	const Colour middle = Sample(image, 0.5, 0.5, Edge::Clamp, Edge::Clamp);
	Check(Near(middle.r, 50.0, 1e-9) && Near(middle.g, 100.0, 1e-9) && Near(middle.a, 191.25, 1e-9), "between the four pixels");
	Check(Near(Sample(image, 1.5, 0.0, Edge::Wrap, Edge::Clamp).r, 50.0, 1e-9), "wrapping reads the first column again");
	Check(Near(Sample(image, 1.5, 0.0, Edge::Clamp, Edge::Clamp).r, 100.0, 1e-9), "clamping stays on the last");
	Check(Near(Sample(image, -0.5, 0.0, Edge::Wrap, Edge::Clamp).r, 50.0, 1e-9), "left of the picture wraps too");
	Check(Near(Sample(image, 0.0, -3.0, Edge::Clamp, Edge::Clamp).g, 0.0, 1e-9), "above the picture clamps");
	Check(Sample(Image{}, 1.0, 1.0, Edge::Wrap, Edge::Wrap).a == 0.0, "an empty picture is clear");
}

Rgb DomeAt(int x, int y, int width) {
	return DarkDomeAt(DomeRowAt((y + 0.5) / (width / 2)), (x + 0.5) / width, 5);
}

bool IsColour(const Rgb& c, double r, double g, double b) {
	return Near(c.r, r, 1e-4) && Near(c.g, g, 1e-4) && Near(c.b, b, 1e-4);
}

// The values are the preview's (artifacts/dome-preview/dome-preview.py, dark_dome(256) and dark_floor).
void TestPainter() {
	std::printf("Painter\n");
	Check(Near(Fbm(0.3, 0.2, 6, 5, 3), 0.376609642, 1e-8) && Near(Fbm(0.999, 0.45, 6, 7, 3), 0.689876846, 1e-8),
	      "the noise is the preview's");
	Check(IsColour(DomeAt(0, 0, 256), 7.172282, 6.427049, 7.884880), "the zenith");
	Check(IsColour(DomeAt(10, 20, 256), 14.779638, 11.459114, 10.493351), "the high sky");
	Check(IsColour(DomeAt(77, 31, 256), 8.707179, 7.583074, 8.068335), "the nebula");
	Check(IsColour(DomeAt(100, 60, 256), 64.310669, 45.846588, 24.706125), "the glow");
	Check(IsColour(DomeAt(200, 63, 256), 81.590397, 57.718752, 29.877743), "just above the horizon");
	Check(IsColour(DomeAt(133, 64, 256), 81.766485, 57.878222, 29.963511), "the ornament's diamond");
	Check(IsColour(DomeAt(3, 65, 256), 78.233821, 55.399920, 28.780868), "the ornament's line below the horizon");
	Check(IsColour(DomeAt(128, 100, 256), 13.584861, 10.671464, 7.859684), "below the horizon");
	Check(IsColour(DomeAt(255, 127, 256), 12.541501, 10.000284, 7.518867), "the nadir");
	Check(IsColour(DarkFloorAt(0.0, 0.0), 45.425261, 33.036553, 19.615454), "the floor under the feet");
	Check(IsColour(DarkFloorAt(0.3, 0.0), 81.093398, 61.804484, 34.404856), "the floor's ring");
	Check(IsColour(DarkFloorAt(0.2, -0.25), 34.909154, 26.105559, 15.860298), "the floor's light");
	Check(IsColour(DarkFloorAt(0.7, 0.5), 11.117944, 9.082646, 7.043884), "the floor's fading edge");
	Check(IsColour(DarkFloorAt(-0.95, 0.2), 11.001920, 9.001340, 7.000702), "the floor's rim");
	FbmRow fast;
	bool same = true;
	for (double v : {0.0, 0.13, 0.31, 0.4999}) {
		PrepareFbmRow(fast, v, 6, 7, 3);
		for (double u : {0.0, 0.001, 0.37, 0.5, 0.99999}) {
			same = same && Near(FbmAt(fast, u), Fbm(u, v, 6, 7, 3), 1e-12);
		}
	}
	Check(same, "a row's noise worked out once is the same noise");
	const DomeRow low = DomeRowAt(0.8);
	Check(!low.nearHorizon && Ornament(low, 0.3) == 0.0, "no ornament away from the horizon");
	Check(Near(DarkDomeAt(DomeRowAt(0.3), 0.0, 5).r, DarkDomeAt(DomeRowAt(0.3), 1.0, 5).r, 1e-9), "no seam at u = 0 / 1");
	Check(Dither(10.49, 0, 0) == 10 && Dither(10.49, 1, 1) == 10 && Dither(10.5, 2, 1) == 11, "dithering rounds by the cell");
	Check(Dither(-4.0, 0, 0) == 0 && Dither(300.0, 0, 0) == 255, "and stays in a byte");
}

Image Flat(int size, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
	Image image;
	image.width = size;
	image.height = size;
	for (int i = 0; i < size * size; ++i) {
		image.rgba.insert(image.rgba.end(), {r, g, b, a});
	}
	return image;
}

void TestGameLayers() {
	std::printf("The game's sky\n");
	const Image stars = Flat(4, 10, 20, 30, 255);
	const Image nebula = Flat(4, 100, 0, 0, 255);
	GameSky sky{&stars, &stars, &nebula};
	Rgb c;
	AddGameSky(GameSkyRowAt(sky, 0.0), 0.2, c);  // the zenith: stars at full, no nebula
	Check(Near(c.r, 10 * 0.9 + 10 * 1.3, 1e-9) && Near(c.b, 30 * 2.2, 1e-9), "both star fields at the zenith");
	c = Rgb{};
	AddGameSky(GameSkyRowAt(sky, 0.5 - 20.0 / 180.0), 0.2, c);  // 20 degrees up: the band's middle
	Check(c.r > 100 * 1.6 && c.g < 20.0, "the nebula's band is strongest 20 degrees up");
	c = Rgb{};
	AddGameSky(GameSkyRowAt(sky, 0.7), 0.2, c);
	Check(c.r == 0.0 && c.g == 0.0, "nothing of the sky below the horizon");

	// A tile of two columns and two rows, read between its pixels and round its edges.
	Image tile;
	tile.width = 2;
	tile.height = 2;
	tile.rgba = {0, 0, 0, 255, 100, 0, 0, 255, 0, 200, 0, 255, 100, 200, 0, 255};
	Rgb t;
	AddTile(TileRowAt(tile, 1.0, 0.5, true, 1.0), 0.25, t);  // x 0.5 of 2, y 0.5
	Check(Near(t.r, 50.0, 1e-9) && Near(t.g, 100.0, 1e-9), "a tile between its four pixels, as Sample reads it");
	t = Rgb{};
	AddTile(TileRowAt(tile, 1.0, 0.0, true, 2.0), 0.75, t);  // x 1.5: the last column and the first again
	Check(Near(t.r, 100.0, 1e-9), "round the tile's right edge, times its gain");
	t = Rgb{};
	AddTile(TileRowAt(tile, 4.0, 1.5, true, 1.0), 0.0625, t);  // four times round: x 0.5; y 1.5 wraps to the first row
	Check(Near(t.r, 50.0, 1e-9) && Near(t.g, 100.0, 1e-9), "tiled round the circle and over the tile's lower edge");
	t = Rgb{};
	AddTile(TileRowAt(tile, 1.0, 1.5, false, 1.0), 0.0, t);
	Check(Near(t.g, 200.0, 1e-9), "the band does not wrap downwards");

	const Image disc = Flat(8, 200, 100, 50, 255);
	const Moon ahead = PlaceMoon(&disc, 0.0, 0.0, 10.0);
	Check(Near(ahead.u, 0.75, 1e-9) && Near(ahead.v, 0.5, 1e-9), "azimuth 0 is u = 0.75, straight ahead on the mesh");
	Check(Near(ahead.right[0], 1.0, 1e-9) && Near(ahead.up[1], 1.0, 1e-9), "its right is the viewer's right");
	const Moon right = PlaceMoon(&disc, 90.0, 30.0, 10.0);
	Check(Near(right.u, 0.0, 1e-9) || Near(right.u, 1.0, 1e-9), "azimuth 90 is the seam, to the right");
	Rgb on{1.0, 1.0, 1.0};
	BlendMoon(ahead, 0.75, 0.5, on);
	Check(Near(on.r, 170.0, 1e-9) && Near(on.g, 85.0, 1e-9), "its middle covers the sky, a little dimmed");
	Rgb off{1.0, 1.0, 1.0};
	BlendMoon(ahead, 0.75 + 8.0 / 360.0, 0.5, off);
	Check(off.r == 1.0, "past its edge the sky stays");
	Rgb seam{1.0, 1.0, 1.0};
	BlendMoon(right, 0.999, 0.5 - 30.0 / 180.0, seam);
	Rgb seam2{1.0, 1.0, 1.0};
	BlendMoon(right, 0.001, 0.5 - 30.0 / 180.0, seam2);
	Check(seam.r > 100.0 && seam2.r > 100.0, "a moon on the seam shows on both sides of it");
	Rgb behind{1.0, 1.0, 1.0};
	BlendMoon(ahead, 0.25, 0.5, behind);
	Check(behind.r == 1.0, "nothing of it behind the viewer");
	const Image clear = Flat(8, 200, 100, 50, 0);
	Rgb through{1.0, 1.0, 1.0};
	BlendMoon(PlaceMoon(&clear, 0.0, 0.0, 10.0), 0.75, 0.5, through);
	Check(through.r == 1.0, "a clear pixel of the moon leaves the sky");
	Rgb none{1.0, 1.0, 1.0};
	BlendMoon(Moon{}, 0.75, 0.5, none);
	Check(none.r == 1.0, "no picture: no moon");

	const Image map = Flat(16, 200, 200, 200, 255);
	Check(Near(MapFloorAt(map, 0.0, 0.0).r, 200 * 0.85, 1e-9), "the map lit under the feet");
	Check(IsColour(MapFloorAt(map, 0.99, 0.0), 11.0, 9.0, 7.0) || MapFloorAt(map, 0.99, 0.0).r < 11.5, "the rim is the dome's floor");
	Check(MapFloorAt(map, 0.95, 0.0).r < MapFloorAt(map, 0.5, 0.0).r, "beside the map the floor is dark");
}

void TestAtlas() {
	std::printf("Atlas\n");
	const AtlasLayout l = LayoutFor(8192);
	Check(l.width == 8192 && l.height == 4096 && l.domeRows == 2560 && l.floorSize == 1536, "8192 across: 2560 rows of sky, a 1536 floor");
	Check(Near(static_cast<double>(l.domeRows) / l.height, kDomeLowestV, 1e-12), "the sky ends 22.5 degrees below the horizon");
	const AtlasLayout s = LayoutFor(256);
	Check(Near(DomeRingV(s, 0), 1.0, 1e-12) && Near(DomeRingV(s, 24), 0.5, 1e-12), "the zenith at the top, the horizon half way");
	Check(Near(DomeRingV(s, 30), 1.0 - 79.5 / 128.0, 1e-12) && DomeRingV(s, 48) == DomeRingV(s, 30),
	      "from the sky's lowest row down the mesh keeps that row");

	const StarField field = MakeStarField(s);
	Check(field.stars.size() == 5600 && field.rows.size() == 80, "5600 stars, a list a row of the sky");
	bool inside = true;
	bool sorted = true;
	for (const Star& star : field.stars) {
		inside = inside && star.x >= 0.0 && star.x < 256.0 && star.y >= 0.0 && star.y < 64.0 && star.sigmaX >= star.sigmaY;
	}
	for (std::size_t y = 0; y < field.rows.size(); ++y) {
		for (std::uint32_t index : field.rows[y]) {
			sorted = sorted && std::fabs(field.stars[index].y - static_cast<double>(y)) <= std::ceil(field.stars[index].sigmaY * 3.0) + 1.0;
		}
	}
	Check(inside, "every star above the horizon, wider towards the zenith");
	Check(sorted, "a row lists only the stars that reach it");
	Check(MakeStarField(s).stars[17].x == field.stars[17].x, "the same stars every time");
	std::vector<double> row(256 * 3, 0.0);
	Star edge;
	edge.x = 0.2;
	edge.y = 5.5;
	edge.light = 1.0;
	edge.sigmaX = 1.0;
	edge.sigmaY = 1.0;
	AddStarToRow(edge, 5, 256, row.data());
	Check(row[0] > 100.0 && row[255 * 3] > 50.0 && row[1] < row[0], "a star at the seam lights both ends of the row, a little warm");

	DomeScene dark;
	dark.stars = field;
	std::vector<std::uint8_t> rgb(256 * 3);
	PaintAtlasRow(dark, s, 64, row.data(), rgb.data());
	Check(rgb[133 * 3] > 70 && rgb[133 * 3 + 2] < 40, "a row of the sky: the horizon's glow");
	PaintAtlasRow(dark, s, 80 + 24, row.data(), rgb.data());
	Check(rgb[24 * 3] > 40 && rgb[100 * 3] == 11 && rgb[255 * 3 + 1] == 9, "a row below it: the floor's light, flat beside the square");
	Check(SceneComplete(dark), "the dark look needs no picture");
	DomeScene sky;
	sky.look = DomeLook::Sky;
	Check(!SceneComplete(sky), "the sky look without its pictures is not complete");

	const std::string obj = DomeObjText(s, "dome_x");
	std::size_t v = 0;
	std::size_t vt = 0;
	std::size_t f = 0;
	for (std::size_t at = 0; at < obj.size(); at = obj.find('\n', at) + 1) {
		v += obj.compare(at, 2, "v ") == 0 ? 1 : 0;
		vt += obj.compare(at, 3, "vt ") == 0 ? 1 : 0;
		f += obj.compare(at, 2, "f ") == 0 ? 1 : 0;
	}
	Check(v == 97 * 49 + 98 && vt == v, "the sphere's and the floor's vertices, a texture point each");
	Check(f == 96 * 48 * 2 + 96, "two triangles a patch and the floor's fan");
	Check(obj.find("mtllib dome_x.mtl\n") != std::string::npos && obj.find("usemtl stage\n") != std::string::npos,
	      "the model names its material");
	Check(obj.find("vt 0.09375 0.18750\n") != std::string::npos, "the floor's middle is the square's (24 of 256, 104 of 128 down)");
	Check(DomeMtlText("dome_x").find("map_Kd dome_x.png\n") != std::string::npos, "the material names the atlas");
}

// A PNG's chunks checked and its pixels unpacked: no filter but the three the writer uses.
bool ReadPng(const std::vector<std::uint8_t>& png, int width, int height, std::vector<std::uint8_t>& pixels) {
	if (png.size() < 8 || png[1] != 'P') {
		return false;
	}
	std::vector<std::uint8_t> packed;
	std::size_t at = 8;
	bool ended = false;
	while (at + 12 <= png.size() && !ended) {
		const std::uint32_t size = (png[at] << 24) | (png[at + 1] << 16) | (png[at + 2] << 8) | png[at + 3];
		if (at + 12 + size > png.size()) {
			return false;
		}
		const std::uint32_t crc = (png[at + 8 + size] << 24) | (png[at + 9 + size] << 16) | (png[at + 10 + size] << 8) | png[at + 11 + size];
		if (static_cast<std::uint32_t>(mz_crc32(MZ_CRC32_INIT, png.data() + at + 4, size + 4)) != crc) {
			return false;
		}
		if (std::memcmp(png.data() + at + 4, "IDAT", 4) == 0) {
			packed.insert(packed.end(), png.begin() + at + 8, png.begin() + at + 8 + size);
		}
		ended = std::memcmp(png.data() + at + 4, "IEND", 4) == 0;
		at += 12 + size;
	}
	const std::size_t rowBytes = static_cast<std::size_t>(width) * 3;
	std::vector<std::uint8_t> raw((rowBytes + 1) * height);
	mz_ulong unpacked = static_cast<mz_ulong>(raw.size());
	if (!ended || at != png.size() || mz_uncompress(raw.data(), &unpacked, packed.data(), static_cast<mz_ulong>(packed.size())) != MZ_OK ||
	    unpacked != raw.size()) {
		return false;
	}
	pixels.assign(rowBytes * height, 0);
	for (int y = 0; y < height; ++y) {
		const std::uint8_t type = raw[(rowBytes + 1) * y];
		for (std::size_t i = 0; i < rowBytes; ++i) {
			const std::uint8_t before = type == 1 ? (i >= 3 ? pixels[rowBytes * y + i - 3] : 0)
			                          : type == 2 ? (y > 0 ? pixels[rowBytes * (y - 1) + i] : 0) : 0;
			pixels[rowBytes * y + i] = static_cast<std::uint8_t>(raw[(rowBytes + 1) * y + 1 + i] + before);
		}
	}
	return true;
}

void TestPng() {
	std::printf("PNG\n");
	const int width = 300;
	const int height = 500;  // enough noise for more than one IDAT chunk
	std::vector<std::uint8_t> picture(static_cast<std::size_t>(width) * height * 3);
	std::uint32_t state = 5;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width * 3; ++x) {
			const double noise = NextUnit(state);
			picture[(static_cast<std::size_t>(y) * width * 3) + x] =
				y < 100 ? static_cast<std::uint8_t>(x) : y < 200 ? static_cast<std::uint8_t>(y) : static_cast<std::uint8_t>(noise * 255);
		}
	}
	std::vector<std::uint8_t> png;
	PngStream stream;
	const auto sink = [&png](const void* bytes, std::size_t count) {
		const std::uint8_t* from = static_cast<const std::uint8_t*>(bytes);
		png.insert(png.end(), from, from + count);
		return true;
	};
	bool ok = stream.Open(sink, width, height, 6);
	for (int y = 0; y < height && ok; ++y) {
		ok = stream.WriteRow(picture.data() + static_cast<std::size_t>(y) * width * 3);
	}
	Check(ok && stream.Close(), "a picture goes through row by row");
	std::vector<std::uint8_t> back;
	Check(ReadPng(png, width, height, back) && back == picture, "read back: every chunk's sum right, every pixel the same");
	std::size_t idats = 0;
	for (std::size_t at = 0; at + 4 <= png.size(); ++at) {
		idats += std::memcmp(png.data() + at, "IDAT", 4) == 0 ? 1 : 0;
	}
	Check(idats > 1 && png.size() < picture.size(), "in several chunks, smaller than its pixels");
	Check(!stream.WriteRow(picture.data()) && !stream.Close(), "closed: nothing more is taken");

	PngStream early;
	std::vector<std::uint8_t> sinkBytes;
	Check(early.Open(sink, 4, 2, 6) && early.WriteRow(picture.data()) && !early.Close(), "closing with a row missing fails");
	PngStream extra;
	Check(extra.Open(sink, 4, 1, 6) && extra.WriteRow(picture.data()) && !extra.WriteRow(picture.data()), "a row too many is refused");
	PngStream none;
	Check(!none.Open(sink, 0, 4, 6) && !none.Open(PngStream::Sink(), 4, 4, 6) && !none.WriteRow(picture.data()),
	      "no size or no sink: not opened");
	PngStream full;
	Check(!full.Open([](const void*, std::size_t) { return false; }, 4, 4, 6), "a sink that refuses fails the opening");
	int writes = 0;
	PngStream late;
	bool lateOk = late.Open([&writes](const void*, std::size_t) { return ++writes <= 4; }, width, 300, 6);
	for (int y = 0; y < 300 && lateOk; ++y) {
		lateOk = late.WriteRow(picture.data() + static_cast<std::size_t>(200 + y) * width * 3);
	}
	Check(!(lateOk && late.Close()), "a sink that refuses later fails the writing");
}

void TestKey() {
	std::printf("The kept dome's key\n");
	TextureStamp stamps[2] = {{"textures\\sky\\skystars.dds", true, false, 100, 200}, {"textures\\sky\\masser_full.dds", true, true, 5, 6}};
	const std::string key = GameDomeKey(8192, stamps, 2);
	Check(key.find("8192 across") != std::string::npos && key.find("skystars.dds archive 100 200\n") != std::string::npos &&
	          key.find("masser_full.dds file 5 6\n") != std::string::npos,
	      "the key names the width and each texture's home, time and size");
	Check(PlanGameDome(key, key, true) == GameDomePlan::Use, "the same key and the dome there: used");
	Check(PlanGameDome(key, key, false) == GameDomePlan::Make, "the same key but a file gone: made again");
	Check(PlanGameDome(key, "", true) == GameDomePlan::Make, "no kept key: made");
	stamps[1].size = 7;
	Check(GameDomeKey(8192, stamps, 2) != key && PlanGameDome(GameDomeKey(8192, stamps, 2), key, true) == GameDomePlan::Make,
	      "a texture changed: made again");
	stamps[1].size = 6;
	stamps[1].loose = false;
	Check(GameDomeKey(8192, stamps, 2) != key, "a texture mod removed changes the key");
	Check(GameDomeKey(4096, stamps, 2) != GameDomeKey(8192, stamps, 2), "another width is another key");
	stamps[0].found = false;
	Check(GameDomeKey(8192, stamps, 2).empty() && PlanGameDome("", key, true) == GameDomePlan::GiveUp,
	      "a texture found nowhere: no key, the dark dome stays");
}

void TestLook() {
	std::printf("The look's word\n");
	DomeLook look = DomeLook::Sky;
	Check(ParseDomeLook("dark", look) && look == DomeLook::Dark, "dark");
	Check(ParseDomeLook("sky", look) && look == DomeLook::Sky, "sky");
	look = DomeLook::Dark;
	Check(!ParseDomeLook("bright", look) && look == DomeLook::Dark, "an unknown word changes nothing");
	Check(DomeLookFromIndex(0.0f) == DomeLook::Sky && DomeLookFromIndex(1.0f) == DomeLook::Dark, "the menu's index");
	Check(DomeLookFromIndex(7.0f) == kDomeLookDefault && DomeLookFromIndex(-1.0f) == kDomeLookDefault && kDomeLookDefault == DomeLook::Dark,
	      "out of range is the default, the dark dome");
}

std::vector<std::uint8_t> FlatDds(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
	std::vector<std::uint8_t> d = DdsHeader(4, 4, 0, 32, true);
	for (int i = 0; i < 16; ++i) {
		d.insert(d.end(), {b, g, r, a});
	}
	return d;
}

std::vector<std::uint8_t> Packed(const std::vector<std::uint8_t>& bytes) {
	mz_ulong size = mz_compressBound(static_cast<mz_ulong>(bytes.size()));
	std::vector<std::uint8_t> out(4 + size);
	for (int i = 0; i < 4; ++i) {
		out[i] = static_cast<std::uint8_t>(bytes.size() >> (8 * i));
	}
	mz_compress(out.data() + 4, &size, bytes.data(), static_cast<mz_ulong>(bytes.size()));
	out.resize(4 + size);
	return out;
}

bool WriteFile(const std::string& path, const std::vector<std::uint8_t>& bytes) {
	std::FILE* f = std::fopen(path.c_str(), "wb");
	const bool ok = f != nullptr && std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
	if (f != nullptr) {
		std::fclose(f);
	}
	return ok;
}

std::vector<std::uint8_t> ReadFile(const std::string& path) {
	std::vector<std::uint8_t> bytes;
	std::FILE* f = std::fopen(path.c_str(), "rb");
	if (f != nullptr) {
		int c;
		while ((c = std::fgetc(f)) != EOF) {
			bytes.push_back(static_cast<std::uint8_t>(c));
		}
		std::fclose(f);
	}
	return bytes;
}

// A Data folder: the archive with all six textures (the map stored, the rest
// packed), the stars white, the map grey.
void TestBuild(const std::string& dir) {
	std::printf("A dome written\n");
	const std::vector<std::uint8_t> white = FlatDds(255, 255, 255, 255);
	const std::vector<ArchiveFile> files = {
		{"textures\\sky", "skystars.dds", Packed(white), false},
		{"textures\\sky", "skystars01.dds", Packed(FlatDds(0, 0, 0, 255)), false},
		{"textures\\sky", "skynebula01.dds", Packed(FlatDds(0, 0, 0, 255)), false},
		{"textures\\sky", "masser_full.dds", Packed(FlatDds(200, 0, 0, 255)), false},
		{"textures\\sky", "secunda_full.dds", Packed(FlatDds(0, 200, 0, 255)), false},
		{"textures\\menus\\map\\world", "cyrodiil_resized.dds", FlatDds(128, 128, 128, 255), true},
	};
	Check(WriteFile(JoinDataPath(dir.c_str(), kGameTextureArchive), MakeArchive(files, 0x7)), "the archive is in place");
	GameTextures textures;
	std::string why;
	Check(LoadGameTextures(dir.c_str(), textures, why) && textures.images[0].At(0, 0)[0] == 255 && textures.images[3].At(0, 0)[0] == 200 &&
	          textures.images[5].At(0, 0)[1] == 128,
	      "all six from the archive, packed and stored");
	Check(!GameTextureIsLoose(dir.c_str(), kGameTexturePaths[0]), "none has its own file");

	Check(WriteDome(DomeLook::Sky, 256, &textures, dir.c_str(), "dome_sky", 6, why), "the sky dome is written");
	std::vector<std::uint8_t> pixels;
	Check(ReadPng(ReadFile(dir + "/dome_sky.png"), 256, 128, pixels), "its atlas reads back");
	Check(pixels[(10 * 256 + 5) * 3] > 200, "white stars high in the sky");
	const int masserX = static_cast<int>(PlaceMoon(nullptr, 35.0, 24.0, 18.0).u * 256);
	const int masserY = static_cast<int>((0.5 - 24.0 / 180.0) * 128);
	Check(pixels[(masserY * 256 + masserX) * 3] > 150 && pixels[(masserY * 256 + masserX) * 3 + 1] < 20, "Masser where it belongs, red here");
	Check(pixels[((80 + 24) * 256 + 24) * 3] > 90 && pixels[((80 + 24) * 256 + 24) * 3 + 1] > 90, "the grey map under the feet");
	Check(ReadFile(dir + "/dome_sky.obj").size() > 100000 && ReadFile(dir + "/dome_sky.mtl").size() > 50 &&
	          ReadFile(dir + "/dome_sky.png.part").empty(),
	      "with its model and material, nothing unfinished left");

	Check(WriteDome(DomeLook::Dark, 256, nullptr, dir.c_str(), "dome_dark", 6, why) &&
	          ReadPng(ReadFile(dir + "/dome_dark.png"), 256, 128, pixels) && pixels[(64 * 256 + 133) * 3] > 70,
	      "the dark dome needs no texture");
	Check(!WriteDome(DomeLook::Sky, 256, nullptr, dir.c_str(), "dome_none", 6, why) && !why.empty() &&
	          ReadFile(dir + "/dome_none.png").empty(),
	      "the sky dome without textures is refused, nothing written");
	Check(!WriteDome(DomeLook::Dark, 100, nullptr, dir.c_str(), "dome_odd", 6, why), "a width that does not divide is refused");
	Check(!WriteDome(DomeLook::Dark, 256, nullptr, (dir + "/no/such/folder").c_str(), "dome", 6, why), "a folder that is not there fails");

	// A texture mod's own file wins over the archive's.
	const std::string loose = JoinDataPath(dir.c_str(), kGameTexturePaths[0]);
	Check(WriteFile(loose, FlatDds(1, 2, 3, 255)) && GameTextureIsLoose(dir.c_str(), kGameTexturePaths[0]) &&
	          LoadGameTextures(dir.c_str(), textures, why) && textures.images[0].At(0, 0)[0] == 1,
	      "a texture's own file under Data is taken before the archive's");
	Check(WriteFile(loose, {1, 2, 3}) && !LoadGameTextures(dir.c_str(), textures, why) && why.find("not a DDS") != std::string::npos,
	      "a file that is no DDS fails with the reason");
	std::remove(loose.c_str());

	std::vector<ArchiveFile> fewer(files.begin(), files.begin() + 5);
	WriteFile(JoinDataPath(dir.c_str(), kGameTextureArchive), MakeArchive(fewer, 0x7));
	Check(!LoadGameTextures(dir.c_str(), textures, why) && why.find("cyrodiil_resized") != std::string::npos,
	      "a texture found nowhere fails and is named");
	std::vector<ArchiveFile> damaged = files;
	damaged[0].bytes.resize(damaged[0].bytes.size() - 6);
	WriteFile(JoinDataPath(dir.c_str(), kGameTextureArchive), MakeArchive(damaged, 0x7));
	Check(!LoadGameTextures(dir.c_str(), textures, why) && why.find("unpacked") != std::string::npos, "a packed file cut short fails");
	std::remove(JoinDataPath(dir.c_str(), kGameTextureArchive).c_str());
	Check(!LoadGameTextures(dir.c_str(), textures, why), "no archive and no files: fails");
}

}  // namespace

int main(int argc, char** argv) {
	TestDds();
	TestArchive();
	TestSampling();
	TestPainter();
	TestGameLayers();
	TestAtlas();
	TestPng();
	TestKey();
	TestLook();
	if (argc > 1) {
		TestBuild(argv[1]);  // a folder with textures/sky made by the build (tests/CMakeLists.txt)
	} else {
		std::printf("  FAIL  no working folder given\n");
		++g_failures;
	}
	std::printf(g_failures == 0 ? "All dome checks passed.\n" : "%d dome checks FAILED.\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
