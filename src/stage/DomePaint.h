#pragma once

// The dome's painter: what colour the loading stage has at a point of its
// sky and of its floor (vr/LoadingStage.h). OBVR's own picture - nearly
// black, a faint warm nebula, a sepia glow along the horizon with a thin
// ornament in it, a soft light under the feet. The same picture
// tools/make-loading-stage.ps1 painted until 2026-11-15, moved here so that
// the shipped dome (tools/make-loading-stage.cpp) and the one made from the
// game's textures in the player's game (stage/DomeGameLayers.h) are painted
// by one code.
//
// A point of the sky is (u, v): u round the circle, 0 to 1, and v from the
// zenith (0) over the horizon (0.5) to the nadir (1). Colours run 0 to 255.

#include <cmath>
#include <cstdint>
#include <vector>

#include "stage/DomeImage.h"

namespace obvr::stage {

inline constexpr double kPi = 3.14159265358979323846;

struct Rgb {
	double r = 0.0;
	double g = 0.0;
	double b = 0.0;
};

inline constexpr Rgb kZenithColour{5.0, 5.0, 7.0};
inline constexpr Rgb kSkyColour{15.0, 12.0, 10.0};
inline constexpr Rgb kGlowColour{82.0, 58.0, 30.0};
inline constexpr Rgb kFloorColour{11.0, 9.0, 7.0};
inline constexpr Rgb kOrnamentColour{120.0, 92.0, 50.0};
inline constexpr Rgb kUnderFeetColour{44.0, 32.0, 19.0};

inline double Clamp01(double t) { return t < 0.0 ? 0.0 : t > 1.0 ? 1.0 : t; }
inline double Smooth(double t) { return t * t * (3.0 - 2.0 * t); }
inline double Mix(double a, double b, double t) { return a + (b - a) * Clamp01(t); }
inline Rgb Mix(const Rgb& a, const Rgb& b, double t) { return {Mix(a.r, b.r, t), Mix(a.g, b.g, t), Mix(a.b, b.b, t)}; }

// Value noise on a lattice that wraps every 'period' cells in x, so the
// dome's seam at u = 0/1 does not show.
inline double NoiseHash(int x, int y, int seed) {
	std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u +
	                  static_cast<std::uint32_t>(seed) * 2147483647u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h ^= h >> 16;
	return (h & 0xFFFFFFu) / static_cast<double>(0xFFFFFF);
}

inline double Noise(double x, double y, int period, int seed) {
	const double floorX = std::floor(x);
	const double floorY = std::floor(y);
	const int x0 = static_cast<int>(floorX);
	const int y0 = static_cast<int>(floorY);
	const double fx = Smooth(x - floorX);
	const double fy = Smooth(y - floorY);
	const int xa = ((x0 % period) + period) % period;
	const int xb = (xa + 1) % period;
	const double a = NoiseHash(xa, y0, seed);
	const double b = NoiseHash(xb, y0, seed);
	const double c = NoiseHash(xa, y0 + 1, seed);
	const double d = NoiseHash(xb, y0 + 1, seed);
	return (a + (b - a) * fx) * (1.0 - fy) + (c + (d - c) * fx) * fy;
}

// Fractal sum, 0 to 1, the period doubling with each octave so every octave wraps.
inline double Fbm(double u, double v, int basePeriod, int octaves, int seed) {
	double sum = 0.0;
	double amp = 0.5;
	double norm = 0.0;
	int period = basePeriod;
	for (int o = 0; o < octaves; ++o) {
		sum += amp * Noise(u * period, v * period, period, seed + o * 17);
		norm += amp;
		amp *= 0.5;
		period *= 2;
	}
	return sum / norm;
}

// The same sum along one row of the picture, v fixed: every octave's lattice
// row is worked out once (the two rows of hashes already mixed by v), and a
// pixel only mixes two neighbours of it. A row of 8192 pixels took 11.2 ms
// through Fbm and takes 0.12 ms this way (measured 2026-11-15, clang -m32 -O2).
struct FbmRow {
	static constexpr int kMostOctaves = 8;
	int octaves = 0;
	int basePeriod = 1;
	std::vector<double> lattice[kMostOctaves];  // period + 1 values an octave, the last the first again
};

inline void PrepareFbmRow(FbmRow& row, double v, int basePeriod, int octaves, int seed) {
	row.octaves = octaves < FbmRow::kMostOctaves ? octaves : FbmRow::kMostOctaves;
	row.basePeriod = basePeriod;
	int period = basePeriod;
	for (int o = 0; o < row.octaves; ++o) {
		const double y = v * period;
		const double floorY = std::floor(y);
		const int y0 = static_cast<int>(floorY);
		const double fy = Smooth(y - floorY);
		std::vector<double>& values = row.lattice[o];
		values.resize(static_cast<std::size_t>(period) + 1);
		for (int x = 0; x < period; ++x) {
			values[static_cast<std::size_t>(x)] =
				NoiseHash(x, y0, seed + o * 17) * (1.0 - fy) + NoiseHash(x, y0 + 1, seed + o * 17) * fy;
		}
		values[static_cast<std::size_t>(period)] = values[0];
		period *= 2;
	}
}

// u from 0 to just under 1.
inline double FbmAt(const FbmRow& row, double u) {
	double sum = 0.0;
	double amp = 0.5;
	double norm = 0.0;
	int period = row.basePeriod;
	for (int o = 0; o < row.octaves; ++o) {
		const double x = u * period;
		const int x0 = static_cast<int>(x);
		const double fx = Smooth(x - x0);
		const double* values = row.lattice[o].data() + x0;
		sum += amp * (values[0] + (values[1] - values[0]) * fx);
		norm += amp;
		amp *= 0.5;
		period *= 2;
	}
	return sum / norm;
}

// What a row of the sky shares: everything that depends on v alone, so the
// row's pixels do not work it out again.
struct DomeRow {
	double v = 0.0;
	double degrees = 0.0;      // below the horizon, negative above
	double band = 0.0;         // the horizon's glow
	double up = 0.0;           // how strongly the nebula shows
	bool nearHorizon = false;  // the ornament's rows
	double line = 0.0;         // the ornament's two thin lines
	Rgb base;
};

inline DomeRow DomeRowAt(double v) {
	DomeRow row;
	row.v = v;
	row.degrees = (v - 0.5) * 180.0;
	const double fromHorizon = std::fabs(v - 0.5) * 2.0;
	row.band = std::exp(-std::pow(fromHorizon / 0.10, 2.0));
	row.up = v < 0.5 ? (fromHorizon * 2.0 < 1.0 ? fromHorizon * 2.0 : 1.0) : 0.25;
	row.base = v < 0.5 ? Mix(kSkyColour, kZenithColour, std::pow(fromHorizon, 0.7)) : kFloorColour;
	row.nearHorizon = std::fabs(row.degrees) < 2.4;  // past it the lines' bell is below a thousandth of a level
	row.line = row.nearHorizon ? std::exp(-std::pow((std::fabs(row.degrees) - 1.6) / 0.10, 2.0)) : 0.0;
	return row;
}

// The ornament's strength at a point of a row near the horizon: two thin
// lines just above and below it, small diamonds between them, 48 round the
// circle, a dot between every two.
inline double Ornament(const DomeRow& row, double u) {
	if (!row.nearHorizon) {
		return 0.0;
	}
	double cell = u * 48.0;
	cell -= std::floor(cell);
	const double du = std::fabs(cell - 0.5) * 7.5;  // degrees along the band, about
	const double diamond = (std::fabs(row.degrees) + du) < 1.05 ? 1.0 : 0.0;
	const double side = (cell < 0.5 ? cell : 1.0 - cell) * 7.5 / 0.35;
	const double dot = std::exp(-(std::pow(row.degrees / 0.35, 2.0) + side * side));
	const double shapes = diamond * 0.85 > dot * 0.6 ? diamond * 0.85 : dot * 0.6;
	return row.line > shapes ? row.line : shapes;
}

// The dark dome: the sky's fall to the zenith, the nebula with its warm
// tint, the glow and the ornament. octaves is the nebula's detail: 5 at 4096
// across, 7 at 8192.
inline constexpr int kNebulaPeriod = 6;
inline constexpr int kNebulaSeed = 3;

// The dark dome at u of a row, cloud the nebula's noise there.
inline Rgb DarkDomeFrom(const DomeRow& row, double u, double cloud) {
	const double gain = 1.0 + (cloud - 0.5) * 0.9 * row.up;
	const double warm = (cloud > 0.52 ? cloud - 0.52 : 0.0) * row.up * 40.0;
	Rgb c{row.base.r * gain + warm, row.base.g * gain + warm * 0.6, row.base.b * gain + warm * 0.25};
	c = Mix(c, kGlowColour, row.band);
	return Mix(c, kOrnamentColour, Ornament(row, u) * 0.75);
}

// A row's nebula made ready: PrepareFbmRow for the dome.
inline void PrepareNebulaRow(FbmRow& nebula, const DomeRow& row, int octaves) {
	PrepareFbmRow(nebula, row.v * 0.5, kNebulaPeriod, octaves, kNebulaSeed);
}

// One point at a time, for a single value (the tests'); a row goes through
// PrepareNebulaRow and DarkDomeFrom.
inline Rgb DarkDomeAt(const DomeRow& row, double u, int octaves) {
	return DarkDomeFrom(row, u, Fbm(u, row.v * 0.5, kNebulaPeriod, octaves, kNebulaSeed));
}

// The dark floor at (px, pz), each -1 to 1 across the disc: the light under
// the feet, a double ring round it, a grain, the edge fading into the dome's
// own floor colour.
inline Rgb DarkFloorAt(double px, double pz) {
	const double rr = std::sqrt(px * px + pz * pz);
	const double light = std::exp(-std::pow(rr / 0.42, 2.0));
	const double ring = std::exp(-std::pow((rr - 0.30) / 0.006, 2.0)) * 0.8 +
	                    std::exp(-std::pow((rr - 0.315) / 0.004, 2.0)) * 0.5;
	const double grain = (Fbm((px + 1.0) * 0.5, (pz + 1.0) * 0.5, 16, 4, 21) - 0.5) * 0.25;
	const double edge = Smooth(Clamp01((rr - 0.55) / 0.45));
	Rgb c = Mix(kFloorColour, kUnderFeetColour, light);
	const double grained = 1.0 + grain * (1.0 - edge);
	c = {c.r * grained, c.g * grained, c.b * grained};
	c = Mix(c, kOrnamentColour, ring * (1.0 - edge) * 0.7);
	return Mix(c, kFloorColour, edge);
}

// Ordered dithering against banding on the long gradients.
inline std::uint8_t Dither(double value, int x, int y) {
	static constexpr int kBayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
	const double d = (kBayer[(y & 3) * 4 + (x & 3)] + 0.5) / 16.0 - 0.5;
	const int v = FloorToInt(value + 0.5 + d);
	return static_cast<std::uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v);
}

}  // namespace obvr::stage
