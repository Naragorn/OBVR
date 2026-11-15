#pragma once

// A decoded picture and how the dome's painter reads it (stage/DomePaint.h):
// eight bits a channel, red first, row by row from the top.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace obvr::stage {

struct Image {
	int width = 0;
	int height = 0;
	std::vector<std::uint8_t> rgba;  // width * height * 4

	bool Empty() const { return width <= 0 || height <= 0 || rgba.size() < static_cast<std::size_t>(width) * height * 4; }
	const std::uint8_t* At(int x, int y) const { return rgba.data() + (static_cast<std::size_t>(y) * width + x) * 4; }
};

// A colour in the painter's own range, 0 to 255 a channel, not yet rounded.
struct Colour {
	double r = 0.0;
	double g = 0.0;
	double b = 0.0;
	double a = 0.0;
};

enum class Edge { Wrap, Clamp };

// The whole number at or below x, without the C library's call (a pixel takes several).
inline int FloorToInt(double x) {
	const int i = static_cast<int>(x);
	return i > x ? i - 1 : i;
}

inline int EdgeIndex(int i, int size, Edge edge) {
	if (edge == Edge::Wrap) {
		i %= size;
		return i < 0 ? i + size : i;
	}
	return i < 0 ? 0 : i >= size ? size - 1 : i;
}

// The picture at (x, y) in its own pixels, between the four around it. An
// empty picture is black and clear.
inline Colour Sample(const Image& image, double x, double y, Edge edgeX, Edge edgeY) {
	if (image.Empty()) {
		return {};
	}
	const int floorX = FloorToInt(x);
	const int floorY = FloorToInt(y);
	const double fx = x - floorX;
	const double fy = y - floorY;
	const int x0 = EdgeIndex(floorX, image.width, edgeX);
	const int x1 = EdgeIndex(floorX + 1, image.width, edgeX);
	const int y0 = EdgeIndex(floorY, image.height, edgeY);
	const int y1 = EdgeIndex(floorY + 1, image.height, edgeY);
	const std::uint8_t* p00 = image.At(x0, y0);
	const std::uint8_t* p10 = image.At(x1, y0);
	const std::uint8_t* p01 = image.At(x0, y1);
	const std::uint8_t* p11 = image.At(x1, y1);
	double channel[4];
	for (int c = 0; c < 4; ++c) {
		const double top = p00[c] + (p10[c] - p00[c]) * fx;
		const double bottom = p01[c] + (p11[c] - p01[c]) * fx;
		channel[c] = top + (bottom - top) * fy;
	}
	return {channel[0], channel[1], channel[2], channel[3]};
}

}  // namespace obvr::stage
