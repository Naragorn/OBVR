// Checks the by-register palette probe: the bucket selection that decides
// where a constant upload is counted, and the formatter that turns four
// snapshots of the buckets into one log line.
//
// The probe exists to separate camera registers, whose sums must differ
// between the two world renders, from bone palette registers, whose sums
// must not. That reading is only as good as the bookkeeping under it:
// buckets must never move once claimed, or snapshot subtraction lies, and
// the formatter must keep a truncated line terminated, or the log call
// after it reads past the buffer.

#include <cstdio>
#include <cstring>

#include "render/PaletteBuckets.h"

namespace {

using obvr::render::FormatPaletteRegisterDeltas;
using obvr::render::PaletteRegisterBucket;
using obvr::render::SelectPaletteBucket;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void TestBucketSelection() {
	std::printf("Bucket selection\n");

	PaletteRegisterBucket buckets[3];
	Check(SelectPaletteBucket(buckets, 3, 40) == 0,
	      "an empty array offers its first bucket");

	// The caller claims the bucket the way the hook does.
	buckets[0].startRegister = 40;
	buckets[0].calls = 1;
	Check(SelectPaletteBucket(buckets, 3, 40) == 0,
	      "the same register finds its claimed bucket again");
	Check(SelectPaletteBucket(buckets, 3, 7) == 1,
	      "a new register is offered the first free bucket");

	buckets[1].startRegister = 7;
	buckets[1].calls = 1;
	buckets[2].startRegister = 90;
	buckets[2].calls = 1;
	Check(SelectPaletteBucket(buckets, 3, 90) == 2,
	      "a full array still finds a claimed register");
	Check(SelectPaletteBucket(buckets, 3, 13) == 3,
	      "a full array refuses a new register with the overflow index");

	// Register 0 is a real register, not an empty marker: claimed by calls,
	// not by its number.
	PaletteRegisterBucket zero[2];
	Check(SelectPaletteBucket(zero, 2, 0) == 0, "register zero claims a bucket like any other");
	zero[0].startRegister = 0;
	zero[0].calls = 1;
	Check(SelectPaletteBucket(zero, 2, 0) == 0, "and finds it again once claimed");
}

// Four snapshots the way the scene hook takes them: entry, after the first
// render, after the moment between, after the second render.
struct Snapshots {
	PaletteRegisterBucket entry[3];
	PaletteRegisterBucket afterFirst[3];
	PaletteRegisterBucket afterBetween[3];
	PaletteRegisterBucket afterSecond[3];
};

void TestFormatterQuietFrame() {
	std::printf("Formatting a frame with no palette uploads\n");

	Snapshots s;
	char out[256];
	out[0] = 'x';
	Check(FormatPaletteRegisterDeltas(s.entry, s.afterFirst, s.afterBetween, s.afterSecond, 3,
	                                  out, sizeof(out)) == 0,
	      "no active register reports zero");
	Check(out[0] == '\0', "and leaves an empty string, not stale bytes");
}

void TestFormatterOnePass() {
	std::printf("Formatting a register active in one render only\n");

	Snapshots s;
	// Register 40 uploads once during the first render; the second render
	// never touches it. All later snapshots carry the accumulated state.
	PaletteRegisterBucket* snaps[] = {s.afterFirst, s.afterBetween, s.afterSecond};
	for (PaletteRegisterBucket* snap : snaps) {
		snap[0].startRegister = 40;
		snap[0].calls = 1;
		snap[0].vectors = 48;
		snap[0].sum = 0xABCD;
	}

	char out[256];
	Check(FormatPaletteRegisterDeltas(s.entry, s.afterFirst, s.afterBetween, s.afterSecond, 3,
	                                  out, sizeof(out)) == 1,
	      "one register active in one pass still counts");
	Check(std::strcmp(out, "c40 first 1/48/0000ABCD second 0/0/00000000") == 0,
	      "the silent pass shows as zeroes rather than vanishing");
}

void TestFormatterBothPassesAndLateClaim() {
	std::printf("Formatting both renders, one bucket claimed mid-frame\n");

	Snapshots s;
	// Register 40, claimed before the frame: both renders upload 48 vectors,
	// the second render's bytes differ - the collapse signature.
	s.entry[0].startRegister = 40;
	s.entry[0].calls = 10;
	s.entry[0].vectors = 480;
	s.entry[0].sum = 0x1000;
	PaletteRegisterBucket* snaps[] = {s.afterFirst, s.afterBetween};
	for (PaletteRegisterBucket* snap : snaps) {
		snap[0].startRegister = 40;
		snap[0].calls = 11;
		snap[0].vectors = 528;
		snap[0].sum = 0x1111;
	}
	s.afterSecond[0].startRegister = 40;
	s.afterSecond[0].calls = 12;
	s.afterSecond[0].vectors = 576;
	s.afterSecond[0].sum = 0x1300;

	// Register 7, first seen during the second render: the entry snapshot
	// never saw the bucket, so its register name only exists in the last one.
	s.afterSecond[1].startRegister = 7;
	s.afterSecond[1].calls = 1;
	s.afterSecond[1].vectors = 16;
	s.afterSecond[1].sum = 0x22;

	char out[256];
	Check(FormatPaletteRegisterDeltas(s.entry, s.afterFirst, s.afterBetween, s.afterSecond, 3,
	                                  out, sizeof(out)) == 2,
	      "two active registers report two");
	Check(std::strcmp(out, "c40 first 1/48/00000111 second 1/48/000001EF, "
	                       "c7 first 0/0/00000000 second 1/16/00000022") == 0,
	      "deltas subtract per register and the late claim is named from the last snapshot");
}

void TestFormatterRefusals() {
	std::printf("Formatting into buffers that cannot hold the line\n");

	Snapshots s;
	PaletteRegisterBucket* snaps[] = {s.afterFirst, s.afterBetween, s.afterSecond};
	for (PaletteRegisterBucket* snap : snaps) {
		snap[0].startRegister = 40;
		snap[0].calls = 1;
		snap[0].vectors = 48;
		snap[0].sum = 0xABCD;
		snap[1].startRegister = 7;
		snap[1].calls = 2;
		snap[1].vectors = 24;
		snap[1].sum = 0x22;
	}

	// Room for the first register but not the second: the count only covers
	// what was actually written, and the line stays terminated.
	char small[48];
	Check(FormatPaletteRegisterDeltas(s.entry, s.afterFirst, s.afterBetween, s.afterSecond, 3,
	                                  small, sizeof(small)) == 1,
	      "a buffer with room for one register reports one");
	Check(std::strlen(small) < sizeof(small), "and stays terminated inside the buffer");
	Check(std::strncmp(small, "c40 first 1/48/0000ABCD", 23) == 0,
	      "what fits is the complete first register");

	Check(FormatPaletteRegisterDeltas(s.entry, s.afterFirst, s.afterBetween, s.afterSecond, 3,
	                                  nullptr, 64) == 0,
	      "a null buffer is refused");
	char untouched[4] = {'x', 'y', 'z', '\0'};
	Check(FormatPaletteRegisterDeltas(s.entry, s.afterFirst, s.afterBetween, s.afterSecond, 3,
	                                  untouched, 0) == 0,
	      "a zero-sized buffer is refused");
	Check(std::strcmp(untouched, "xyz") == 0, "and left untouched");
}

}  // namespace

int main() {
	std::printf("OBVR palette buckets test\n\n");

	TestBucketSelection();
	std::printf("\n");
	TestFormatterQuietFrame();
	std::printf("\n");
	TestFormatterOnePass();
	std::printf("\n");
	TestFormatterBothPassesAndLateClaim();
	std::printf("\n");
	TestFormatterRefusals();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
