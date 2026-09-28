#include "render/DdsImage.h"

namespace obvr::render {
namespace {

UInt32 Word(const UInt8* p) {
	return static_cast<UInt32>(p[0]) | (static_cast<UInt32>(p[1]) << 8) |
	       (static_cast<UInt32>(p[2]) << 16) | (static_cast<UInt32>(p[3]) << 24);
}

UInt32 Half(const UInt8* p) { return static_cast<UInt32>(p[0]) | (static_cast<UInt32>(p[1]) << 8); }

constexpr UInt32 FourCc(char a, char b, char c, char d) {
	return static_cast<UInt32>(static_cast<UInt8>(a)) | (static_cast<UInt32>(static_cast<UInt8>(b)) << 8) |
	       (static_cast<UInt32>(static_cast<UInt8>(c)) << 16) |
	       (static_cast<UInt32>(static_cast<UInt8>(d)) << 24);
}

// DDS_PIXELFORMAT's flags.
constexpr UInt32 kPfAlphaPixels = 0x1;
constexpr UInt32 kPfFourCc = 0x4;
constexpr UInt32 kPfRgb = 0x40;

UInt32 BlockBytes(DdsInfo::Format f) { return f == DdsInfo::Format::Dxt1 ? 8u : 16u; }

UInt32 TopLevelBytes(const DdsInfo& info) {
	if (info.format == DdsInfo::Format::Masked) {
		return info.width * info.height * (info.bitCount / 8);
	}
	return ((info.width + 3) / 4) * ((info.height + 3) / 4) * BlockBytes(info.format);
}

// A 5:6:5 colour widened to eight bits a channel.
Pixel Expand565(UInt32 c) {
	const UInt32 r = (c >> 11) & 0x1F;
	const UInt32 g = (c >> 5) & 0x3F;
	const UInt32 b = c & 0x1F;
	return Pixel{static_cast<UInt8>((r << 3) | (r >> 2)), static_cast<UInt8>((g << 2) | (g >> 4)),
	             static_cast<UInt8>((b << 3) | (b >> 2)), 255};
}

Pixel Mix(Pixel a, Pixel b, UInt32 wa, UInt32 wb) {
	const UInt32 total = wa + wb;
	return Pixel{static_cast<UInt8>((a.r * wa + b.r * wb) / total),
	             static_cast<UInt8>((a.g * wa + b.g * wb) / total),
	             static_cast<UInt8>((a.b * wa + b.b * wb) / total), 255};
}

// The four colours of a colour block; `allowTransparent` is BC1's mode.
void ColourTable(const UInt8* block, bool allowTransparent, Pixel (&table)[4]) {
	const UInt32 c0 = Half(block);
	const UInt32 c1 = Half(block + 2);
	table[0] = Expand565(c0);
	table[1] = Expand565(c1);
	if (c0 > c1 || !allowTransparent) {
		table[2] = Mix(table[0], table[1], 2, 1);
		table[3] = Mix(table[0], table[1], 1, 2);
	} else {
		table[2] = Mix(table[0], table[1], 1, 1);
		table[3] = Pixel{0, 0, 0, 0};
	}
}

void AlphaTable(UInt32 a0, UInt32 a1, UInt8 (&table)[8]) {
	table[0] = static_cast<UInt8>(a0);
	table[1] = static_cast<UInt8>(a1);
	if (a0 > a1) {
		for (UInt32 i = 1; i <= 6; ++i) {
			table[i + 1] = static_cast<UInt8>(((7 - i) * a0 + i * a1) / 7);
		}
	} else {
		for (UInt32 i = 1; i <= 4; ++i) {
			table[i + 1] = static_cast<UInt8>(((5 - i) * a0 + i * a1) / 5);
		}
		table[6] = 0;
		table[7] = 255;
	}
}

// rowPitch: the bytes from one row of blocks to the next (a file packs them;
// a locked surface may pad).
void DecodeBlocks(const DdsInfo& info, const UInt8* data, UInt32 rowPitch, Pixel* out) {
	const UInt32 blocksWide = (info.width + 3) / 4;
	const UInt32 blocksHigh = (info.height + 3) / 4;
	const UInt32 stride = BlockBytes(info.format);
	for (UInt32 by = 0; by < blocksHigh; ++by) {
		for (UInt32 bx = 0; bx < blocksWide; ++bx) {
			const UInt8* block = data + by * rowPitch + bx * stride;
			const UInt8* colour = info.format == DdsInfo::Format::Dxt1 ? block : block + 8;
			Pixel table[4];
			ColourTable(colour, info.format == DdsInfo::Format::Dxt1, table);
			const UInt32 indices = Word(colour + 4);
			UInt8 alphas[8] = {};
			UInt32 alphaLow = 0;   // DXT5's 48 index bits, low 24
			UInt32 alphaHigh = 0;  // and high 24
			if (info.format == DdsInfo::Format::Dxt5) {
				AlphaTable(block[0], block[1], alphas);
				alphaLow = static_cast<UInt32>(block[2]) | (static_cast<UInt32>(block[3]) << 8) |
				           (static_cast<UInt32>(block[4]) << 16);
				alphaHigh = static_cast<UInt32>(block[5]) | (static_cast<UInt32>(block[6]) << 8) |
				            (static_cast<UInt32>(block[7]) << 16);
			}
			for (UInt32 i = 0; i < 16; ++i) {
				const UInt32 x = bx * 4 + (i & 3);
				const UInt32 y = by * 4 + (i >> 2);
				if (x >= info.width || y >= info.height) {
					continue;
				}
				Pixel p = table[(indices >> (2 * i)) & 3];
				if (info.format == DdsInfo::Format::Dxt3) {
					const UInt32 nibble = (block[i / 2] >> ((i & 1) * 4)) & 0xF;
					p.a = static_cast<UInt8>(nibble * 17);
				} else if (info.format == DdsInfo::Format::Dxt5) {
					const UInt32 bits = i < 8 ? (alphaLow >> (3 * i)) : (alphaHigh >> (3 * (i - 8)));
					p.a = alphas[bits & 7];
				}
				out[y * info.width + x] = p;
			}
		}
	}
}

// A channel under its mask, widened to eight bits; `absent` for no mask.
UInt8 Channel(UInt32 value, UInt32 mask, UInt8 absent) {
	if (mask == 0) {
		return absent;
	}
	UInt32 shift = 0;
	while (((mask >> shift) & 1) == 0) {
		++shift;
	}
	const UInt32 max = mask >> shift;
	return static_cast<UInt8>((((value & mask) >> shift) * 255 + max / 2) / max);
}

void DecodeMasked(const DdsInfo& info, const UInt8* data, UInt32 rowPitch, Pixel* out) {
	const UInt32 bytes = info.bitCount / 8;
	for (UInt32 i = 0; i < info.width * info.height; ++i) {
		const UInt8* p = data + (i / info.width) * rowPitch + (i % info.width) * bytes;
		UInt32 v = 0;
		for (UInt32 k = 0; k < bytes; ++k) {
			v |= static_cast<UInt32>(p[k]) << (8 * k);
		}
		out[i] = Pixel{Channel(v, info.redMask, 0), Channel(v, info.greenMask, 0),
		               Channel(v, info.blueMask, 0), Channel(v, info.alphaMask, 255)};
	}
}

}  // namespace

bool ReadDdsHeader(const UInt8* data, UInt32 size, DdsInfo& out) {
	out = DdsInfo{};
	if (data == nullptr || size < kDdsHeaderBytes || Word(data) != FourCc('D', 'D', 'S', ' ') ||
	    Word(data + 4) != 124) {
		return false;
	}
	out.height = Word(data + 12);
	out.width = Word(data + 16);
	if (out.width == 0 || out.height == 0 || out.width > kDdsMaxSide || out.height > kDdsMaxSide) {
		return false;
	}
	const UInt32 pfFlags = Word(data + 80);
	if ((pfFlags & kPfFourCc) != 0) {
		const UInt32 fourCc = Word(data + 84);
		if (fourCc == FourCc('D', 'X', 'T', '1')) {
			out.format = DdsInfo::Format::Dxt1;
		} else if (fourCc == FourCc('D', 'X', 'T', '3')) {
			out.format = DdsInfo::Format::Dxt3;
		} else if (fourCc == FourCc('D', 'X', 'T', '5')) {
			out.format = DdsInfo::Format::Dxt5;
		} else {
			return false;  // DX10 headers, DXT2/4, the rest
		}
	} else if ((pfFlags & kPfRgb) != 0) {
		out.format = DdsInfo::Format::Masked;
		out.bitCount = Word(data + 88);
		if (out.bitCount != 16 && out.bitCount != 24 && out.bitCount != 32) {
			return false;
		}
		out.redMask = Word(data + 92);
		out.greenMask = Word(data + 96);
		out.blueMask = Word(data + 100);
		out.alphaMask = (pfFlags & kPfAlphaPixels) != 0 ? Word(data + 104) : 0;
		if (out.redMask == 0 && out.greenMask == 0 && out.blueMask == 0) {
			return false;
		}
	} else {
		return false;  // luminance, alpha only
	}
	return size - kDdsHeaderBytes >= TopLevelBytes(out);
}

bool DecodeDds(const UInt8* data, UInt32 size, std::vector<Pixel>& out, UInt32& width,
               UInt32& height) {
	DdsInfo info;
	width = 0;
	height = 0;
	if (!ReadDdsHeader(data, size, info)) {
		return false;
	}
	out.assign(static_cast<size_t>(info.width) * info.height, Pixel{0, 0, 0, 0});
	if (info.format == DdsInfo::Format::Masked) {
		DecodeMasked(info, data + kDdsHeaderBytes, info.width * (info.bitCount / 8), out.data());
	} else {
		DecodeBlocks(info, data + kDdsHeaderBytes, ((info.width + 3) / 4) * BlockBytes(info.format), out.data());
	}
	width = info.width;
	height = info.height;
	return true;
}

void FitSquare(const Pixel* src, UInt32 width, UInt32 height, Pixel* dst, UInt32 side) {
	for (UInt32 i = 0; i < side * side; ++i) {
		dst[i] = Pixel{0, 0, 0, 0};
	}
	if (src == nullptr || width == 0 || height == 0 || side == 0) {
		return;
	}
	const UInt32 longer = width > height ? width : height;
	// The picture's size in the square, its shape kept, and where it starts.
	const UInt32 placedW = width * side / longer > 0 ? width * side / longer : 1;
	const UInt32 placedH = height * side / longer > 0 ? height * side / longer : 1;
	const UInt32 left = (side - placedW) / 2;
	const UInt32 top = (side - placedH) / 2;
	for (UInt32 dy = 0; dy < placedH; ++dy) {
		UInt32 y0 = dy * longer / side;
		UInt32 y1 = (dy + 1) * longer / side;
		if (y1 <= y0) {
			y1 = y0 + 1;
		}
		if (y1 > height) {
			y1 = height;
		}
		for (UInt32 dx = 0; dx < placedW; ++dx) {
			UInt32 x0 = dx * longer / side;
			UInt32 x1 = (dx + 1) * longer / side;
			if (x1 <= x0) {
				x1 = x0 + 1;
			}
			if (x1 > width) {
				x1 = width;
			}
			UInt32 count = 0;
			UInt32 sumA = 0;
			UInt32 sumR = 0;
			UInt32 sumG = 0;
			UInt32 sumB = 0;
			for (UInt32 y = y0; y < y1; ++y) {
				for (UInt32 x = x0; x < x1; ++x) {
					const Pixel p = src[y * width + x];
					++count;
					sumA += p.a;
					sumR += p.r * p.a;
					sumG += p.g * p.a;
					sumB += p.b * p.a;
				}
			}
			Pixel& d = dst[(top + dy) * side + left + dx];
			if (count == 0 || sumA == 0) {
				continue;
			}
			d = Pixel{static_cast<UInt8>(sumR / sumA), static_cast<UInt8>(sumG / sumA),
			          static_cast<UInt8>(sumB / sumA), static_cast<UInt8>(sumA / count)};
		}
	}
}

}  // namespace obvr::render

namespace obvr::render {

bool SurfaceInfo(UInt32 d3dFormat, UInt32 width, UInt32 height, DdsInfo& out) {
	out = DdsInfo{};
	if (width == 0 || height == 0 || width > kDdsMaxSide || height > kDdsMaxSide) {
		return false;
	}
	out.width = width;
	out.height = height;
	switch (d3dFormat) {
	case kD3dFormatDxt1: out.format = DdsInfo::Format::Dxt1; return true;
	case kD3dFormatDxt3: out.format = DdsInfo::Format::Dxt3; return true;
	case kD3dFormatDxt5: out.format = DdsInfo::Format::Dxt5; return true;
	default: break;
	}
	out.format = DdsInfo::Format::Masked;
	switch (d3dFormat) {
	case kD3dFormatA8R8G8B8:
		out.bitCount = 32; out.redMask = 0x00FF0000; out.greenMask = 0x0000FF00; out.blueMask = 0x000000FF;
		out.alphaMask = 0xFF000000; return true;
	case kD3dFormatX8R8G8B8:
		out.bitCount = 32; out.redMask = 0x00FF0000; out.greenMask = 0x0000FF00; out.blueMask = 0x000000FF;
		return true;
	case kD3dFormatR8G8B8:
		out.bitCount = 24; out.redMask = 0x00FF0000; out.greenMask = 0x0000FF00; out.blueMask = 0x000000FF;
		return true;
	case kD3dFormatR5G6B5:
		out.bitCount = 16; out.redMask = 0xF800; out.greenMask = 0x07E0; out.blueMask = 0x001F; return true;
	case kD3dFormatA1R5G5B5:
		out.bitCount = 16; out.redMask = 0x7C00; out.greenMask = 0x03E0; out.blueMask = 0x001F;
		out.alphaMask = 0x8000; return true;
	case kD3dFormatA4R4G4B4:
		out.bitCount = 16; out.redMask = 0x0F00; out.greenMask = 0x00F0; out.blueMask = 0x000F;
		out.alphaMask = 0xF000; return true;
	default:
		return false;
	}
}

bool DecodeSurface(UInt32 d3dFormat, const UInt8* bits, UInt32 pitch, UInt32 width, UInt32 height,
                   std::vector<Pixel>& out) {
	DdsInfo info;
	if (bits == nullptr || !SurfaceInfo(d3dFormat, width, height, info)) {
		return false;
	}
	const UInt32 rowBytes = info.format == DdsInfo::Format::Masked ? width * (info.bitCount / 8)
	                                                                 : ((width + 3) / 4) * BlockBytes(info.format);
	if (pitch < rowBytes) {
		return false;
	}
	out.assign(static_cast<size_t>(width) * height, Pixel{0, 0, 0, 0});
	if (info.format == DdsInfo::Format::Masked) {
		DecodeMasked(info, bits, pitch, out.data());
	} else {
		DecodeBlocks(info, bits, pitch, out.data());
	}
	return true;
}

Pixel AverageColour(const Pixel* pixels, UInt32 count) {
	if (pixels == nullptr || count == 0) {
		return Pixel{0, 0, 0, 0};
	}
	UInt32 r = 0;
	UInt32 g = 0;
	UInt32 b = 0;
	for (UInt32 i = 0; i < count; ++i) {
		r += pixels[i].r;
		g += pixels[i].g;
		b += pixels[i].b;
	}
	return Pixel{static_cast<UInt8>(r / count), static_cast<UInt8>(g / count), static_cast<UInt8>(b / count), 255};
}

}  // namespace obvr::render
