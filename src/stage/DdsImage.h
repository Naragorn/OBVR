#pragma once

// A DDS file's largest picture decoded (stage/DomeImage.h): what the game's
// own sky and map textures are kept in. DXT1, DXT3 and DXT5 (the vanilla
// sky, moons and map are all DXT5 - read off "Oblivion - Textures -
// Compressed.bsa", 2026-11-15) and plain 32 or 24 bits a pixel, blue first,
// for a texture mod's file. The layouts are Microsoft's "DDS_HEADER" and
// "Block Compression" pages.

#include "stage/DomeImage.h"

namespace obvr::stage {

inline constexpr std::size_t kDdsHeaderBytes = 128;  // "DDS " and the 124-byte header
inline constexpr int kDdsLargestSide = 8192;

inline std::uint32_t ReadU32(const std::uint8_t* p) {
	return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
	       (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

inline void Rgb565(std::uint32_t v, std::uint8_t out[4]) {
	out[0] = static_cast<std::uint8_t>(((v >> 11) & 31) * 255 / 31);
	out[1] = static_cast<std::uint8_t>(((v >> 5) & 63) * 255 / 63);
	out[2] = static_cast<std::uint8_t>((v & 31) * 255 / 31);
	out[3] = 255;
}

// The eight bytes of colour every DXT block ends with, into sixteen pixels.
// punchThrough is DXT1's own rule: with the first endpoint not the larger,
// the fourth colour is clear black.
inline void DecodeColourBlock(const std::uint8_t* block, bool punchThrough, std::uint8_t out[16][4]) {
	const std::uint32_t c0 = block[0] | (static_cast<std::uint32_t>(block[1]) << 8);
	const std::uint32_t c1 = block[2] | (static_cast<std::uint32_t>(block[3]) << 8);
	std::uint8_t colours[4][4];
	Rgb565(c0, colours[0]);
	Rgb565(c1, colours[1]);
	const bool four = c0 > c1 || !punchThrough;
	for (int c = 0; c < 3; ++c) {
		colours[2][c] = static_cast<std::uint8_t>(four ? (2 * colours[0][c] + colours[1][c]) / 3
		                                              : (colours[0][c] + colours[1][c]) / 2);
		colours[3][c] = static_cast<std::uint8_t>(four ? (colours[0][c] + 2 * colours[1][c]) / 3 : 0);
	}
	colours[2][3] = 255;
	colours[3][3] = static_cast<std::uint8_t>(four ? 255 : 0);
	const std::uint32_t indices = ReadU32(block + 4);
	for (int i = 0; i < 16; ++i) {
		const std::uint8_t* from = colours[(indices >> (2 * i)) & 3];
		for (int c = 0; c < 4; ++c) {
			out[i][c] = from[c];
		}
	}
}

// DXT5's eight bytes of alpha: two endpoints and sixteen three-bit indices.
inline void DecodeDxt5Alpha(const std::uint8_t* block, std::uint8_t out[16][4]) {
	const std::uint32_t a0 = block[0];
	const std::uint32_t a1 = block[1];
	std::uint8_t alphas[8];
	alphas[0] = static_cast<std::uint8_t>(a0);
	alphas[1] = static_cast<std::uint8_t>(a1);
	if (a0 > a1) {
		for (std::uint32_t i = 1; i < 7; ++i) {
			alphas[i + 1] = static_cast<std::uint8_t>(((7 - i) * a0 + i * a1) / 7);
		}
	} else {
		for (std::uint32_t i = 1; i < 5; ++i) {
			alphas[i + 1] = static_cast<std::uint8_t>(((5 - i) * a0 + i * a1) / 5);
		}
		alphas[6] = 0;
		alphas[7] = 255;
	}
	std::uint64_t bits = 0;
	for (int i = 0; i < 6; ++i) {
		bits |= static_cast<std::uint64_t>(block[2 + i]) << (8 * i);
	}
	for (int i = 0; i < 16; ++i) {
		out[i][3] = alphas[(bits >> (3 * i)) & 7];
	}
}

// DXT3's eight bytes of alpha: four bits a pixel.
inline void DecodeDxt3Alpha(const std::uint8_t* block, std::uint8_t out[16][4]) {
	for (int i = 0; i < 16; ++i) {
		const std::uint32_t nibble = (block[i / 2] >> ((i & 1) * 4)) & 15;
		out[i][3] = static_cast<std::uint8_t>(nibble * 17);
	}
}

enum class DdsKind { None, Dxt1, Dxt3, Dxt5, Bgra32, Bgr24 };

struct DdsShape {
	DdsKind kind = DdsKind::None;
	int width = 0;
	int height = 0;
	std::size_t dataBytes = 0;  // the largest picture's bytes after the header
};

// What the header says, DdsKind::None for anything this does not decode.
inline DdsShape ReadDdsShape(const std::uint8_t* data, std::size_t size) {
	DdsShape shape;
	if (data == nullptr || size < kDdsHeaderBytes || ReadU32(data) != 0x20534444u || ReadU32(data + 4) != 124u) {
		return shape;
	}
	const std::uint32_t height = ReadU32(data + 12);
	const std::uint32_t width = ReadU32(data + 16);
	if (width == 0 || height == 0 || width > kDdsLargestSide || height > kDdsLargestSide) {
		return shape;
	}
	const std::uint32_t formatFlags = ReadU32(data + 80);
	const std::uint32_t fourCc = ReadU32(data + 84);
	const std::uint32_t bits = ReadU32(data + 88);
	const std::size_t blocks = static_cast<std::size_t>((width + 3) / 4) * ((height + 3) / 4);
	DdsKind kind = DdsKind::None;
	std::size_t bytes = 0;
	if ((formatFlags & 0x4u) != 0) {  // DDPF_FOURCC
		kind = fourCc == 0x31545844u ? DdsKind::Dxt1 : fourCc == 0x33545844u ? DdsKind::Dxt3
		     : fourCc == 0x35545844u ? DdsKind::Dxt5 : DdsKind::None;
		bytes = blocks * (kind == DdsKind::Dxt1 ? 8 : 16);
	} else if ((formatFlags & 0x40u) != 0 && ReadU32(data + 92) == 0x00FF0000u && ReadU32(data + 100) == 0x000000FFu) {
		kind = bits == 32 ? DdsKind::Bgra32 : bits == 24 ? DdsKind::Bgr24 : DdsKind::None;  // DDPF_RGB, blue first
		bytes = static_cast<std::size_t>(width) * height * (bits / 8);
	}
	if (kind == DdsKind::None || size - kDdsHeaderBytes < bytes) {
		return shape;
	}
	shape.kind = kind;
	shape.width = static_cast<int>(width);
	shape.height = static_cast<int>(height);
	shape.dataBytes = bytes;
	return shape;
}

inline void DecodeDxtBlocks(const DdsShape& shape, const std::uint8_t* blocks, Image& out) {
	const std::size_t blockBytes = shape.kind == DdsKind::Dxt1 ? 8 : 16;
	const int across = (shape.width + 3) / 4;
	const int down = (shape.height + 3) / 4;
	for (int by = 0; by < down; ++by) {
		for (int bx = 0; bx < across; ++bx) {
			const std::uint8_t* block = blocks + (static_cast<std::size_t>(by) * across + bx) * blockBytes;
			std::uint8_t pixels[16][4];
			DecodeColourBlock(shape.kind == DdsKind::Dxt1 ? block : block + 8, shape.kind == DdsKind::Dxt1, pixels);
			if (shape.kind == DdsKind::Dxt5) {
				DecodeDxt5Alpha(block, pixels);
			} else if (shape.kind == DdsKind::Dxt3) {
				DecodeDxt3Alpha(block, pixels);
			}
			for (int i = 0; i < 16; ++i) {
				const int x = bx * 4 + (i & 3);
				const int y = by * 4 + (i >> 2);
				if (x < shape.width && y < shape.height) {
					std::uint8_t* to = out.rgba.data() + (static_cast<std::size_t>(y) * shape.width + x) * 4;
					to[0] = pixels[i][0];
					to[1] = pixels[i][1];
					to[2] = pixels[i][2];
					to[3] = pixels[i][3];
				}
			}
		}
	}
}

// The file's largest picture into out; false for a file that is not a DDS
// this knows, or is cut short.
inline bool DecodeDds(const std::uint8_t* data, std::size_t size, Image& out) {
	const DdsShape shape = ReadDdsShape(data, size);
	if (shape.kind == DdsKind::None) {
		return false;
	}
	out.width = shape.width;
	out.height = shape.height;
	out.rgba.assign(static_cast<std::size_t>(shape.width) * shape.height * 4, 0);
	const std::uint8_t* body = data + kDdsHeaderBytes;
	if (shape.kind == DdsKind::Bgra32 || shape.kind == DdsKind::Bgr24) {
		const std::size_t step = shape.kind == DdsKind::Bgra32 ? 4 : 3;
		const bool alpha = step == 4 && (ReadU32(data + 80) & 0x1u) != 0;  // DDPF_ALPHAPIXELS
		const std::size_t pixels = static_cast<std::size_t>(shape.width) * shape.height;
		for (std::size_t i = 0; i < pixels; ++i) {
			out.rgba[i * 4 + 0] = body[i * step + 2];
			out.rgba[i * 4 + 1] = body[i * step + 1];
			out.rgba[i * 4 + 2] = body[i * step + 0];
			out.rgba[i * 4 + 3] = alpha ? body[i * step + 3] : 255;
		}
		return true;
	}
	DecodeDxtBlocks(shape, body, out);
	return true;
}

}  // namespace obvr::stage
