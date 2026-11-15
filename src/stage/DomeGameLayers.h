#pragma once

// The sky made from the game's own textures, laid over the dark dome
// (stage/DomePaint.h): the game's two star fields tiled over the sky, its
// nebula as a band above the horizon, Masser and Secunda, and Cyrodiil's map
// on the floor. The pictures are the player's own game files, read where the
// game runs (stage/DomeBuild.h); the look is the preview Nadi chose on
// 2026-11-15 (artifacts/dome-preview/B-game-sky.png).
//
// A point of the sky is (u, v) as in DomePaint.h. On the dome's mesh
// (stage/DomeAtlas.h) u = 0.75 is straight ahead in SteamVR's standing space
// (-z) and u grows to the right.

#include "stage/DomeImage.h"
#include "stage/DomePaint.h"

namespace obvr::stage {

struct GameSky {
	const Image* stars = nullptr;    // textures\sky\skystars.dds
	const Image* stars01 = nullptr;  // textures\sky\skystars01.dds
	const Image* nebula = nullptr;   // textures\sky\skynebula01.dds
};

// A tiled picture along one row of the sky: the two rows of the tile the
// sky's row falls between, found once.
struct TileRow {
	const std::uint8_t* above = nullptr;
	const std::uint8_t* below = nullptr;
	double fy = 0.0;
	double tiles = 0.0;  // how often the tile goes round the circle
	int width = 0;
	double gain = 0.0;
};

// yInTile: the row in the tile's own pixels, wrapped into it.
inline TileRow TileRowAt(const Image& tile, double tiles, double yInTile, bool wrapY, double gain) {
	TileRow row;
	const int y0 = FloorToInt(yInTile);
	row.above = tile.At(0, EdgeIndex(y0, tile.height, wrapY ? Edge::Wrap : Edge::Clamp));
	row.below = tile.At(0, EdgeIndex(y0 + 1, tile.height, wrapY ? Edge::Wrap : Edge::Clamp));
	row.fy = yInTile - y0;
	row.tiles = tiles;
	row.width = tile.width;
	row.gain = gain;
	return row;
}

// The tile's colour at u, times its gain, added to c. u from 0 to just under 1.
inline void AddTile(const TileRow& row, double u, Rgb& c) {
	const double turns = u * row.tiles;
	const double x = (turns - static_cast<int>(turns)) * row.width;
	const int x0 = static_cast<int>(x);
	const int x1 = x0 + 1 == row.width ? 0 : x0 + 1;
	const double fx = x - x0;
	double channel[3];
	for (int i = 0; i < 3; ++i) {
		const double top = row.above[x0 * 4 + i] + (row.above[x1 * 4 + i] - row.above[x0 * 4 + i]) * fx;
		const double bottom = row.below[x0 * 4 + i] + (row.below[x1 * 4 + i] - row.below[x0 * 4 + i]) * fx;
		channel[i] = top + (bottom - top) * row.fy;
	}
	c.r += channel[0] * row.gain;
	c.g += channel[1] * row.gain;
	c.b += channel[2] * row.gain;
}

// What a row shares for the game's layers: the star fields (the dense one
// ten times round, the bright one six, both fading towards the horizon) and
// the nebula three times round as a band up to 40 degrees, strongest half
// way. Nothing of them below the horizon.
struct GameSkyRow {
	bool stars = false;
	bool nebula = false;
	TileRow dense;
	TileRow bright;
	TileRow band;
};

inline double Fraction(double x) { return x - FloorToInt(x); }

inline GameSkyRow GameSkyRowAt(const GameSky& sky, double v) {
	GameSkyRow row;
	const double starGain = std::pow(Clamp01((0.5 - v) * 2.0), 0.6);
	if (starGain > 0.0) {
		row.stars = true;
		row.dense = TileRowAt(*sky.stars, 10.0, Fraction(v * 5.0) * sky.stars->height, true, 0.9 * starGain);
		row.bright = TileRowAt(*sky.stars01, 6.0, Fraction(v * 3.0) * sky.stars01->height, true, 1.3 * starGain);
	}
	const double elevation = (0.5 - v) * 180.0;
	if (elevation > 0.0 && elevation < 40.0) {
		row.nebula = true;
		row.band = TileRowAt(*sky.nebula, 3.0, Clamp01((40.0 - elevation) / 40.0) * (sky.nebula->height - 1), false,
		                     1.6 * std::sin(Clamp01(elevation / 40.0) * kPi));
	}
	return row;
}

inline void AddGameSky(const GameSkyRow& row, double u, Rgb& c) {
	if (row.stars) {
		AddTile(row.dense, u, c);
		AddTile(row.bright, u, c);
	}
	if (row.nebula) {
		AddTile(row.band, u, c);
	}
}

// The direction of a point of the sky, as the dome's mesh has it: y up.
inline void SkyDirection(double u, double v, double out[3]) {
	const double theta = 2.0 * kPi * u;
	const double phi = kPi * v;
	out[0] = std::sin(phi) * std::cos(theta);
	out[1] = std::cos(phi);
	out[2] = std::sin(phi) * std::sin(theta);
}

// A moon: its picture on a square that faces the dome's middle.
struct Moon {
	const Image* image = nullptr;
	double centre[3] = {0.0, 0.0, -1.0};
	double right[3] = {1.0, 0.0, 0.0};
	double up[3] = {0.0, 1.0, 0.0};
	double half = 0.0;    // the tangent of half its width
	double u = 0.0;       // where its middle is
	double v = 0.0;
	double reachU = 0.0;  // how far from the middle its square can reach
	double reachV = 0.0;
};

// azimuth: degrees to the right of straight ahead; elevation: degrees above
// the horizon; diameter: the square's width in degrees.
inline Moon PlaceMoon(const Image* image, double azimuth, double elevation, double diameter) {
	Moon m;
	m.image = image;
	const double az = azimuth * kPi / 180.0;
	const double el = elevation * kPi / 180.0;
	m.centre[0] = std::sin(az) * std::cos(el);
	m.centre[1] = std::sin(el);
	m.centre[2] = -std::cos(az) * std::cos(el);
	// right = centre x up(0,1,0), so the picture's left is the viewer's left; up = right x centre.
	const double length = std::sqrt(m.centre[0] * m.centre[0] + m.centre[2] * m.centre[2]);
	m.right[0] = -m.centre[2] / length;
	m.right[1] = 0.0;
	m.right[2] = m.centre[0] / length;
	m.up[0] = m.right[1] * m.centre[2] - m.right[2] * m.centre[1];
	m.up[1] = m.right[2] * m.centre[0] - m.right[0] * m.centre[2];
	m.up[2] = m.right[0] * m.centre[1] - m.right[1] * m.centre[0];
	m.half = std::tan(diameter * kPi / 360.0);
	double theta = std::atan2(m.centre[2], m.centre[0]);
	if (theta < 0.0) {
		theta += 2.0 * kPi;
	}
	m.u = theta / (2.0 * kPi);
	m.v = 0.5 - elevation / 180.0;
	const double reach = std::atan(m.half * 1.4143);  // the square's corner
	m.reachV = reach / kPi;
	const double highest = std::fabs(el) + reach;
	const double narrow = highest < 1.5 ? std::cos(highest) : std::cos(1.5);  // the circle shrinks towards the pole
	m.reachU = reach / (2.0 * kPi) / narrow;
	return m;
}

// Whether the point can be on the moon's square at all: the cheap test a
// pixel takes before any direction is worked out.
inline bool MoonNear(const Moon& m, double u, double v) {
	if (std::fabs(v - m.v) > m.reachV) {
		return false;
	}
	double du = std::fabs(u - m.u);
	if (du > 0.5) {
		du = 1.0 - du;
	}
	return du <= m.reachU;
}

// The moon laid over c by its own alpha, a little dimmed.
inline void BlendMoon(const Moon& m, double u, double v, Rgb& c) {
	if (m.image == nullptr || !MoonNear(m, u, v)) {
		return;
	}
	double p[3];
	SkyDirection(u, v, p);
	const double dot = p[0] * m.centre[0] + p[1] * m.centre[1] + p[2] * m.centre[2];
	if (dot <= 1e-6) {
		return;
	}
	const double sx = (p[0] * m.right[0] + p[1] * m.right[1] + p[2] * m.right[2]) / dot / m.half;
	const double sy = (p[0] * m.up[0] + p[1] * m.up[1] + p[2] * m.up[2]) / dot / m.half;
	if (std::fabs(sx) >= 1.0 || std::fabs(sy) >= 1.0) {
		return;
	}
	const Image& image = *m.image;
	const Colour t = Sample(image, (sx + 1.0) * 0.5 * (image.width - 1), (1.0 - sy) * 0.5 * (image.height - 1),
	                        Edge::Clamp, Edge::Clamp);
	const double a = t.a / 255.0;
	c.r = c.r * (1.0 - a) + t.r * 0.85 * a;
	c.g = c.g * (1.0 - a) + t.g * 0.85 * a;
	c.b = c.b * (1.0 - a) + t.b * 0.85 * a;
}

// The floor with Cyrodiil's map on it, (px, pz) each -1 to 1 across the
// disc, the map's top ahead: lit under the feet, the edge fading into the
// dome's floor colour. The map's file is square with the map in its upper
// 1660 of 2048 rows (read off textures\menus\map\world\cyrodiil_resized.dds).
inline Rgb MapFloorAt(const Image& map, double px, double pz) {
	const double kFill = 0.92;  // the map's half width on the disc
	const double mapRows = map.height * (1660.0 / 2048.0);
	const double rr = std::sqrt(px * px + pz * pz);
	Rgb col = kFloorColour;
	if (std::fabs(px) < kFill && std::fabs(pz) < kFill) {
		const Colour s = Sample(map, (px / kFill + 1.0) * 0.5 * (map.width - 1), (pz / kFill + 1.0) * 0.5 * (mapRows - 1.0),
		                        Edge::Clamp, Edge::Clamp);
		col = {s.r, s.g, s.b};
	}
	const double light = 0.30 + 0.55 * std::exp(-std::pow(rr / 0.55, 2.0));
	const double edge = Smooth(Clamp01((rr - 0.6) / 0.4));
	return Mix(Rgb{col.r * light, col.g * light, col.b * light}, kFloorColour, edge);
}

}  // namespace obvr::stage
