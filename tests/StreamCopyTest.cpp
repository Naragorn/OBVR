// Checks the fast stream copy (render/StreamCopyLogic.h) against the
// engine's own loops, read from Oblivion.exe and mirrored in
// StreamCopyReference: every path - zero-fill, indexed, straight, the
// unrolled sizes and the general one - lands the same bytes and returns the
// same total; and the check mode's span covers exactly what a copy touches.

#include <cstdio>
#include <cstring>

#include "render/StreamCopyLogic.h"

using namespace obvr::render;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr UInt32 kSourceBytes = 4096;
constexpr UInt32 kDestBytes = 8192;
UInt8 g_source[kSourceBytes];
UInt16 g_indices[256];
UInt8 g_destReference[kDestBytes];
UInt8 g_destFast[kDestBytes];

void FillSource() {
	for (UInt32 i = 0; i < kSourceBytes; ++i) {
		g_source[i] = static_cast<UInt8>((i * 7u + 3u) & 0xFF);
	}
	for (UInt32 i = 0; i < 256; ++i) {
		g_indices[i] = static_cast<UInt16>((i * 37u + 11u) % 200u);   // a shuffled partition
	}
}

// Runs both copies over fresh destinations (filled with a marker so an
// untouched byte shows) and compares bytes and totals.
bool Same(StreamCopyDesc d, UInt32* referenceTotal = nullptr) {
	std::memset(g_destReference, 0xCD, kDestBytes);
	std::memset(g_destFast, 0xCD, kDestBytes);
	d.dest = g_destReference;
	const UInt32 r = StreamCopyReference(d);
	d.dest = g_destFast;
	const UInt32 f = StreamCopyFast(d);
	if (referenceTotal != nullptr) *referenceTotal = r;
	return r == f && std::memcmp(g_destReference, g_destFast, kDestBytes) == 0;
}

StreamCopyDesc Desc(UInt32 elementBytes, UInt16 count, bool indexed, UInt32 destStride) {
	StreamCopyDesc d{};
	d.word0 = 3;
	d.sourceWords = static_cast<UInt16>((elementBytes + 3) / 4);   // the source holds elements of this size
	d.count = count;
	d.indices = indexed ? g_indices : nullptr;
	d.source = g_source;
	d.elementBytes = elementBytes;
	d.sourceStride = elementBytes;
	d.countedBytes = elementBytes;
	d.destStride = destStride;
	return d;
}

void TestZeroFill() {
	std::printf("Zero-fill (no source)\n");
	StreamCopyDesc d = Desc(12, 50, false, 32);
	d.source = nullptr;
	UInt32 total = 0;
	Check(Same(d, &total) && total == 600, "50 elements of 12 in a 32 stride: zeroed, 600 counted");
	Check(g_destFast[12] == 0xCD && g_destFast[31] == 0xCD && g_destFast[32] == 0,
	      "the bytes between the elements are untouched");
	d = Desc(16, 40, false, 16);
	d.source = nullptr;
	Check(Same(d, &total) && total == 640, "a packed zero-fill (stride equals the size): one memset");
	d = Desc(12, 50, true, 32);
	d.source = nullptr;
	Check(Same(d), "no source wins over an index table");
	// The engine zeroes countedBytes per element, not elementBytes (OBNG,
	// 2026-10-30, re/functions/0x0076E610.md: the memset's length read at
	// 0x0076E642/0x0076E64E is the descriptor's +0x1C).
	d = Desc(12, 20, false, 32);
	d.source = nullptr;
	d.countedBytes = 16;
	Check(Same(d, &total) && total == 320, "zero-fill, 12 copied but 16 counted: the same in both");
	Check(g_destFast[12] == 0 && g_destFast[15] == 0 && g_destFast[16] == 0xCD,
	      "16 bytes zeroed per element, as the engine does");
	Check(StreamCopyDestSpan(d) == 19 * 32 + 16, "its span ends at the counted bytes");
	d = Desc(12, 20, false, 16);
	d.source = nullptr;
	d.countedBytes = 16;
	Check(Same(d, &total) && total == 320 && g_destFast[16 * 20 - 1] == 0 && g_destFast[16 * 20] == 0xCD,
	      "packed by the counted bytes: one memset of count x 16");
}

void TestIndexed() {
	std::printf("Indexed (the skinned bodies' path)\n");
	UInt32 total = 0;
	Check(Same(Desc(12, 200, true, 12), &total) && total == 2400, "float3 positions, packed destination");
	Check(Same(Desc(12, 200, true, 36)), "float3 positions into a 36-byte vertex");
	Check(Same(Desc(8, 150, true, 36)), "float2 texture coordinates into a 36-byte vertex");
	Check(Same(Desc(16, 100, true, 48)), "float4 into a 48-byte vertex");
	Check(Same(Desc(4, 200, true, 20)), "a colour (4 bytes)");
	Check(Same(Desc(20, 60, true, 24)), "an odd 20-byte element takes the general loop");
	StreamCopyDesc d = Desc(12, 100, true, 12);
	d.sourceWords = 5;   // the source holds 20-byte elements, 12 of them wanted
	Check(Same(d), "the index steps the source by its words, not by the element size");
	d = Desc(12, 100, true, 12);
	d.countedBytes = 16;
	Check(Same(d, &total) && total == 1600, "the counted total is per element, whatever the copy size");
	Check(g_destFast[1200] == 0xCD, "nothing is written past the last element");
}

void TestStraight() {
	std::printf("Straight runs\n");
	UInt32 total = 0;
	Check(Same(Desc(8, 120, false, 36), &total) && total == 960, "8 bytes unrolled");
	Check(Same(Desc(12, 120, false, 36)), "12 bytes unrolled");
	Check(Same(Desc(16, 120, false, 48)), "16 bytes unrolled");
	Check(Same(Desc(4, 120, false, 20)), "4 bytes, the general loop in the engine");
	Check(Same(Desc(24, 60, false, 32)), "24 bytes, the general loop");
	StreamCopyDesc d = Desc(12, 100, false, 12);
	d.sourceStride = 40;   // the source is interleaved, the destination packed
	Check(Same(d), "a source stride wider than the element");
}

void TestEmpty() {
	std::printf("Nothing to copy\n");
	UInt32 total = 7;
	Check(Same(Desc(12, 0, true, 12), &total) && total == 0, "count zero: nothing written, zero returned");
	Check(g_destFast[0] == 0xCD, "the destination untouched");
	Check(StreamCopyDestSpan(Desc(12, 0, true, 12)) == 0, "an empty copy spans nothing");
}

void TestSpan() {
	std::printf("The check mode's span\n");
	Check(StreamCopyDestSpan(Desc(12, 1, true, 36)) == 12, "one element: its bytes");
	Check(StreamCopyDestSpan(Desc(12, 10, true, 36)) == 9 * 36 + 12, "ten in a 36 stride: nine strides and one element");
	Check(StreamCopyDestSpan(Desc(16, 10, false, 16)) == 160, "packed: count times the size");
}

void TestLayout() {
	std::printf("The descriptor\n");
	StreamCopyDesc d{};
	const char* base = reinterpret_cast<const char*>(&d);
	Check(reinterpret_cast<const char*>(&d.sourceWords) - base == 0x04, "source words at +4");
	Check(reinterpret_cast<const char*>(&d.count) - base == 0x08, "count at +8");
	Check(reinterpret_cast<const char*>(&d.indices) - base == 0x0C, "indices at +0xC");
	Check(reinterpret_cast<const char*>(&d.source) - base == 0x10, "source at +0x10");
	Check(reinterpret_cast<const char*>(&d.elementBytes) - base == 0x14, "element bytes at +0x14");
	Check(reinterpret_cast<const char*>(&d.sourceStride) - base == 0x18, "source stride at +0x18");
	Check(reinterpret_cast<const char*>(&d.countedBytes) - base == 0x1C, "counted bytes at +0x1C");
	Check(reinterpret_cast<const char*>(&d.destStride) - base == 0x20, "destination stride at +0x20");
	Check(reinterpret_cast<const char*>(&d.dest) - base == 0x24, "destination at +0x24");
}

}  // namespace

int main() {
	FillSource();
	TestLayout();
	TestZeroFill();
	TestIndexed();
	TestStraight();
	TestEmpty();
	TestSpan();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
