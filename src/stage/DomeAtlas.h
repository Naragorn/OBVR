#pragma once

// The dome's one texture and its mesh (vr/LoadingStage.h): SteamVR's stage
// takes an OBJ with one material, so the sky and the floor share an atlas.
//
//   * the atlas is twice as wide as high. Its top 5/8 hold the sky from the
//     zenith to 22.5 degrees below the horizon, at the same pixels a degree
//     in both directions; below that the dome is one flat colour (the
//     horizon's glow has faded by then), so the mesh keeps its lowest row of
//     the picture all the way down instead of spending pixels on it.
//   * the bottom 3/8 hold the floor's square on the left, the rest flat.
//
// At 8192 across that is 22.8 pixels a degree for the sky - the atlas before
// 2026-11-15 was 4096 x 4096 with 11.4, the lower half of the sky's picture
// and a quarter of the atlas flat.
//
// The mesh: a sphere of 20 m around the playspace's origin, faced inwards,
// and a floor disc of 4 m radius just above the origin's floor. y up, metres;
// u = 0.75 is straight ahead (-z), growing to the right.

#include <cstdio>
#include <string>
#include <vector>

#include "stage/DomeGameLayers.h"
#include "stage/DomeLook.h"
#include "stage/DomePaint.h"

namespace obvr::stage {

struct AtlasLayout {
	int width = 0;
	int height = 0;
	int domeRows = 0;   // the sky's rows, from the top
	int floorSize = 0;  // the floor's square, bottom left
};

inline constexpr int kAtlasWidth = 8192;
// The shipped dark dome's width (tools/make-loading-stage.cpp): 34.1 pixels a
// degree. The eye's picture has 26.9 at its middle with the Dream Air (4036
// pixels over tangents 1.402 + 1.213, read in OBVR.log 2026-11-15), so 8192
// (22.8) was shown magnified; 16384 would be 4 times 8192's memory in the
// compositor (512 MB unpacked against 288 MB here) for nothing the eye's
// picture resolves. The sky made in the game stays at kAtlasWidth.
inline constexpr int kDarkAtlasWidth = 12288;
inline constexpr double kDomeLowestV = 0.625;  // where the sky's picture ends

// width: a multiple of 16.
inline AtlasLayout LayoutFor(int width) {
	AtlasLayout l;
	l.width = width;
	l.height = width / 2;
	l.domeRows = width / 16 * 5;
	l.floorSize = l.height - l.domeRows;
	return l;
}

// The painted stars of the dark look, in the sky's pixels.
struct Star {
	double x = 0.0;
	double y = 0.0;
	double light = 0.0;
	double sigmaX = 0.0;  // wider towards the zenith, where a pixel is narrower on the sphere
	double sigmaY = 0.0;
};

struct StarField {
	std::vector<Star> stars;
	std::vector<std::vector<std::uint32_t>> rows;  // which stars touch a row
};

inline double NextUnit(std::uint32_t& state) {  // xorshift32, 0 to just under 1
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return (state >> 8) / 16777216.0;
}

// count stars spread evenly over the sky's sphere above 4.6 degrees. share
// dims them all; a star brighter than 0.8 gets the large size (degrees).
inline void AddStars(StarField& field, const AtlasLayout& l, int count, std::uint32_t seed, double share,
                     double smallDegrees, double largeDegrees) {
	std::uint32_t state = seed * 2654435761u + 1u;
	for (int i = 0; i < count; ++i) {
		const double z = NextUnit(state) * 0.92 + 0.08;  // the sine of the elevation
		const double elevation = std::asin(z);
		const double u = NextUnit(state);
		const double cube = NextUnit(state);
		const double light = (0.25 + 0.75 * cube * cube * cube) * share;
		double sigma = (light > 0.8 ? largeDegrees : smallDegrees) * l.width / 360.0;
		sigma = sigma < 0.5 ? 0.5 : sigma;
		const double narrow = std::cos(elevation) < 0.1 ? 0.1 : std::cos(elevation);
		Star star;
		star.x = u * l.width;
		star.y = (0.5 - elevation / kPi) * l.height;
		star.light = light;
		star.sigmaX = sigma / narrow;
		star.sigmaY = sigma;
		field.stars.push_back(star);
	}
}

// The dark look's stars: 1400 clear ones and 4200 faint small ones.
inline StarField MakeStarField(const AtlasLayout& l) {
	StarField field;
	AddStars(field, l, 1400, 7, 1.0, 0.10, 0.16);
	AddStars(field, l, 4200, 8, 0.45, 0.035, 0.05);
	field.rows.resize(static_cast<std::size_t>(l.domeRows));
	for (std::uint32_t i = 0; i < field.stars.size(); ++i) {
		const Star& s = field.stars[i];
		const int reach = static_cast<int>(std::ceil(s.sigmaY * 3.0));
		const int middle = static_cast<int>(s.y);
		for (int y = middle - reach; y <= middle + reach; ++y) {
			if (y >= 0 && y < l.domeRows) {
				field.rows[static_cast<std::size_t>(y)].push_back(i);
			}
		}
	}
	return field;
}

// One star's light added to a row of colours (three doubles a pixel).
inline void AddStarToRow(const Star& s, int y, int width, double* row) {
	const int reach = static_cast<int>(std::ceil(s.sigmaX * 3.0));
	const int middle = static_cast<int>(s.x);
	const double dy = (y + 0.5 - s.y) / s.sigmaY;
	for (int x = middle - reach; x <= middle + reach; ++x) {
		const double dx = (x + 0.5 - s.x) / s.sigmaX;
		const double light = s.light * 150.0 * std::exp(-(dx * dx + dy * dy) * 0.5);
		double* p = row + static_cast<std::size_t>(((x % width) + width) % width) * 3;
		p[0] += light;
		p[1] += light * 0.95;
		p[2] += light * 0.85;
	}
}

// Everything a dome is painted from. The sky look needs all six pictures.
struct DomeScene {
	DomeLook look = DomeLook::Dark;
	int octaves = 7;
	StarField stars;   // the dark look's
	GameSky sky;       // the sky look's
	Moon moons[2];
	const Image* map = nullptr;
};

inline bool SceneComplete(const DomeScene& scene) {
	if (scene.look == DomeLook::Dark) {
		return true;
	}
	const Image* needed[] = {scene.sky.stars, scene.sky.stars01, scene.sky.nebula,
	                         scene.moons[0].image, scene.moons[1].image, scene.map};
	for (const Image* image : needed) {
		if (image == nullptr || image->Empty()) {
			return false;
		}
	}
	return true;
}

// A row of the sky into work (three doubles a pixel).
inline void PaintSkyRow(const DomeScene& scene, const AtlasLayout& l, int y, double* work) {
	const double v = (y + 0.5) / l.height;
	const DomeRow row = DomeRowAt(v);
	const bool sky = scene.look == DomeLook::Sky;
	const GameSkyRow game = sky ? GameSkyRowAt(scene.sky, v) : GameSkyRow{};
	FbmRow nebula;
	PrepareNebulaRow(nebula, row, scene.octaves);
	for (int x = 0; x < l.width; ++x) {
		const double u = (x + 0.5) / l.width;
		Rgb c = DarkDomeFrom(row, u, FbmAt(nebula, u));
		if (sky) {
			AddGameSky(game, u, c);
			BlendMoon(scene.moons[0], u, v, c);
			BlendMoon(scene.moons[1], u, v, c);
		}
		work[x * 3 + 0] = c.r;
		work[x * 3 + 1] = c.g;
		work[x * 3 + 2] = c.b;
	}
	if (!sky && static_cast<std::size_t>(y) < scene.stars.rows.size()) {
		for (std::uint32_t index : scene.stars.rows[static_cast<std::size_t>(y)]) {
			AddStarToRow(scene.stars.stars[index], y, l.width, work);
		}
	}
}

// A row below the sky into work: the floor's square, flat beside it.
inline void PaintFloorRow(const DomeScene& scene, const AtlasLayout& l, int y, double* work) {
	const double pz = (y - l.domeRows + 0.5) / l.floorSize * 2.0 - 1.0;
	for (int x = 0; x < l.width; ++x) {
		Rgb c = kFloorColour;
		if (x < l.floorSize) {
			const double px = (x + 0.5) / l.floorSize * 2.0 - 1.0;
			c = scene.look == DomeLook::Sky ? MapFloorAt(*scene.map, px, pz) : DarkFloorAt(px, pz);
		}
		work[x * 3 + 0] = c.r;
		work[x * 3 + 1] = c.g;
		work[x * 3 + 2] = c.b;
	}
}

// Row y of the atlas into rgb (three bytes a pixel); work is the caller's
// buffer of width * 3 doubles. The scene must be SceneComplete.
inline void PaintAtlasRow(const DomeScene& scene, const AtlasLayout& l, int y, double* work, std::uint8_t* rgb) {
	if (y < l.domeRows) {
		PaintSkyRow(scene, l, y, work);
	} else {
		PaintFloorRow(scene, l, y, work);
	}
	for (int x = 0; x < l.width; ++x) {
		rgb[x * 3 + 0] = Dither(work[x * 3 + 0], x, y);
		rgb[x * 3 + 1] = Dither(work[x * 3 + 1], x, y);
		rgb[x * 3 + 2] = Dither(work[x * 3 + 2], x, y);
	}
}

inline constexpr int kDomeSegments = 96;
inline constexpr int kDomeRings = 48;
inline constexpr int kFloorSegments = 96;
inline constexpr double kDomeRadiusMetres = 20.0;
inline constexpr double kFloorRadiusMetres = 4.0;
inline constexpr double kFloorHeightMetres = 0.005;

// Where a ring of the sphere (0 the zenith, kDomeRings the nadir) reads the
// atlas, OBJ's v up: the sky's picture down to its lowest row, that row from
// there on.
inline double DomeRingV(const AtlasLayout& l, int ring) {
	const double row = static_cast<double>(ring) / kDomeRings * l.height;
	const double lowest = l.domeRows - 0.5;
	return 1.0 - (row < lowest ? row : lowest) / l.height;
}

inline void AppendLine(std::string& text, const char* format, double a, double b, double c) {
	char line[96];
	std::snprintf(line, sizeof(line), format, a, b, c);
	text += line;
}

inline void AppendSphere(std::string& v, std::string& vt, std::string& vn, const AtlasLayout& l) {
	for (int r = 0; r <= kDomeRings; ++r) {
		const double phi = kPi * r / kDomeRings;
		for (int s = 0; s <= kDomeSegments; ++s) {
			const double theta = 2.0 * kPi * s / kDomeSegments;
			const double x = std::sin(phi) * std::cos(theta);
			const double y = std::cos(phi);
			const double z = std::sin(phi) * std::sin(theta);
			AppendLine(v, "v %.4f %.4f %.4f\n", kDomeRadiusMetres * x, kDomeRadiusMetres * y, kDomeRadiusMetres * z);
			AppendLine(vt, "vt %.5f %.5f\n", static_cast<double>(s) / kDomeSegments, DomeRingV(l, r), 0.0);
			AppendLine(vn, "vn %.4f %.4f %.4f\n", -x, -y, -z);
		}
	}
}

// The floor disc: a centre and a ring, facing up; a texel in from the
// square's edge so the sky's rows above it do not bleed in.
inline void AppendFloor(std::string& v, std::string& vt, std::string& vn, const AtlasLayout& l) {
	const double half = l.floorSize * 0.5;
	const double reach = half - 1.0;
	AppendLine(v, "v 0.0000 %.4f 0.0000\n", kFloorHeightMetres, 0.0, 0.0);
	AppendLine(vt, "vt %.5f %.5f\n", half / l.width, 1.0 - (l.domeRows + half) / l.height, 0.0);
	vn += "vn 0.0000 1.0000 0.0000\n";
	for (int s = 0; s <= kFloorSegments; ++s) {
		const double theta = 2.0 * kPi * s / kFloorSegments;
		AppendLine(v, "v %.4f %.4f %.4f\n", kFloorRadiusMetres * std::cos(theta), kFloorHeightMetres,
		           kFloorRadiusMetres * std::sin(theta));
		AppendLine(vt, "vt %.5f %.5f\n", (half + reach * std::cos(theta)) / l.width,
		           1.0 - (l.domeRows + half + reach * std::sin(theta)) / l.height, 0.0);
		vn += "vn 0.0000 1.0000 0.0000\n";
	}
}

// The model's text: name is the file's name without its ending, the
// material file's and the atlas's too.
inline std::string DomeObjText(const AtlasLayout& l, const char* name) {
	std::string v;
	std::string vt;
	std::string vn;
	AppendSphere(v, vt, vn, l);
	AppendFloor(v, vt, vn, l);
	std::string text = "# OBVR's loading stage (src/stage/DomeAtlas.h): a 20 m dome faced inwards and a 4 m floor disc\n";
	text += std::string("mtllib ") + name + ".mtl\n" + v + vt + vn + "usemtl stage\n";
	char face[96];
	const int row = kDomeSegments + 1;
	for (int r = 0; r < kDomeRings; ++r) {
		for (int s = 0; s < kDomeSegments; ++s) {
			const int a = r * row + s + 1;
			const int b = a + 1;
			const int c = a + row;
			const int d = c + 1;
			std::snprintf(face, sizeof(face), "f %d/%d/%d %d/%d/%d %d/%d/%d\n", a, a, a, c, c, c, b, b, b);  // counter-clockwise from inside
			text += face;
			std::snprintf(face, sizeof(face), "f %d/%d/%d %d/%d/%d %d/%d/%d\n", b, b, b, c, c, c, d, d, d);
			text += face;
		}
	}
	const int centre = (kDomeRings + 1) * row + 1;
	for (int s = 0; s < kFloorSegments; ++s) {
		const int a = centre + 1 + s;
		const int b = a + 1;
		std::snprintf(face, sizeof(face), "f %d/%d/%d %d/%d/%d %d/%d/%d\n", centre, centre, centre, b, b, b, a, a, a);  // from above
		text += face;
	}
	return text;
}

inline std::string DomeMtlText(const char* name) {
	return std::string("# OBVR's loading stage (src/stage/DomeAtlas.h)\nnewmtl stage\nKa 1.000 1.000 1.000\n"
	                   "Kd 1.000 1.000 1.000\nKs 0.000 0.000 0.000\nillum 1\nmap_Kd ") + name + ".png\n";
}

}  // namespace obvr::stage
