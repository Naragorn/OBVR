#include <cstdio>

#include "game/RefFingerprint.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::game::LooksLikeGameVtable;
using obvr::game::RefFingerprint;
using obvr::game::RefFingerprintMatches;
using obvr::game::RefLooksLive;
using obvr::game::ReadablePages;
using obvr::game::ReadSpanAsked;
using obvr::game::SpanReadableBy;
using obvr::game::NextWorld;
using obvr::game::RefFromWorld;

// A reference as TakeRefFingerprint takes it: a vtable inside the game's
// image (Character's, game/GameAddresses.h kVtblCharacter), a dynamic form id.
constexpr UInt32 kRef = 0x1D6A91B0;      // the crash's reference
constexpr UInt32 kVtable = 0x00A6FC9C;
constexpr UInt32 kFormId = 0xFF000A21;

RefFingerprint Taken() {
	RefFingerprint fp;
	fp.ref = kRef;
	fp.vtable = kVtable;
	fp.formId = kFormId;
	return fp;
}

void TestStillThere() {
	std::printf("the same reference, untouched\n");
	Check(RefFingerprintMatches(Taken(), kVtable, kFormId, 0), "same vtable, form id, no flags: still there");
	// Flags other than deleted or disabled say nothing about it being gone
	// (persistent 0x400, a quest item's 0x10000 ...).
	Check(RefFingerprintMatches(Taken(), kVtable, kFormId, 0x400 | 0x10000), "other flags: still there");
}

void TestGone() {
	std::printf("deleted, disabled, or its memory another's\n");
	Check(!RefFingerprintMatches(Taken(), kVtable, kFormId, 0x20), "deleted (0x20): gone");
	Check(!RefFingerprintMatches(Taken(), kVtable, kFormId, 0x800), "disabled (0x800): gone");
	Check(!RefFingerprintMatches(Taken(), kVtable, kFormId, 0x20 | 0x800 | 0x400), "both, among others: gone");
	// Reused by another reference (the dropped weapon's own): same vtable,
	// another form id.
	Check(!RefFingerprintMatches(Taken(), kVtable, 0xFF000A22, 0), "another form id: gone");
	// Reused by another kind of object (HighProcess's vtable, game/SlowApproach.h),
	// or a free list's pointer over the vtable.
	Check(!RefFingerprintMatches(Taken(), 0x00A71814, kFormId, 0), "another vtable: gone");
	Check(!RefFingerprintMatches(Taken(), 0x1D6A9000, kFormId, 0), "a heap pointer where the vtable was: gone");
	// What the crash read: 1.0 where the node had been.
	Check(!RefFingerprintMatches(Taken(), 0x3F800000, 0x3F800000, 0x3F800000), "a float's bytes: gone");
}

void TestEmptyFingerprint() {
	std::printf("a fingerprint never taken matches nothing\n");
	RefFingerprint none;
	Check(!RefFingerprintMatches(none, 0, 0, 0), "empty against zeros: no");
	RefFingerprint noRef = Taken();
	noRef.ref = 0;
	Check(!RefFingerprintMatches(noRef, kVtable, kFormId, 0), "no address, even with all else equal: no");
	// TakeRefFingerprint takes none with a vtable outside the game's image;
	// one made by hand matches nothing either, whatever is read.
	RefFingerprint foreignVtable = Taken();
	foreignVtable.vtable = 0x1D000000;
	Check(!RefFingerprintMatches(foreignVtable, 0x1D000000, kFormId, 0), "a vtable not the game's: no");
}

void TestGameImage() {
	std::printf("the game's image, where every vtable lies (Oblivion.exe: base 0x400000, SizeOfImage 0x805000)\n");
	Check(!LooksLikeGameVtable(0), "null: no");
	Check(!LooksLikeGameVtable(0x003FFFFF), "just below the image: no");
	Check(LooksLikeGameVtable(0x00400000), "the image's first byte: yes");
	Check(LooksLikeGameVtable(kVtable), "Character's vtable: yes");
	Check(LooksLikeGameVtable(0x00A73A0C), "PlayerCharacter's vtable: yes");
	Check(LooksLikeGameVtable(0x00C04FFF), "the image's last byte: yes");
	Check(!LooksLikeGameVtable(0x00C05000), "just past the image: no");
	Check(!LooksLikeGameVtable(0x3F800000), "1.0's bytes: no");
	Check(!LooksLikeGameVtable(0x80000000), "a pointer over 2 GB (large address aware): no");
}

void TestLooksLive() {
	std::printf("a reference nobody remembered (the engine's crosshair slot): live or not\n");
	Check(RefLooksLive(kVtable, 0), "the game's vtable, no flags: live");
	Check(RefLooksLive(kVtable, 0x400 | 0x10000), "other flags: live");
	Check(!RefLooksLive(kVtable, 0x20), "deleted: not");
	Check(!RefLooksLive(kVtable, 0x800), "disabled: not");
	// The crash of 2026-10-12: a float's bytes where the shield's reference
	// had been.
	Check(!RefLooksLive(0x3F7E0010, 0), "a float where the vtable was: not");
	Check(!RefLooksLive(0, 0), "null: not");
	Check(!RefLooksLive(0x1D6A9000, 0), "a heap pointer where the vtable was: not");
}

// A load replaces the world: every reference remembered from the old one is
// gone, whatever its memory holds now - and its memory may not be mapped at
// all any more (the crash of 2026-10-27 15:40, RefStillThere reading the pick
// ring's settled reference after a load out of MemorialCave).
void TestWorld() {
	std::printf("the world a fingerprint was taken in\n");
	RefFingerprint fp = Taken();
	fp.world = 7;
	Check(RefFromWorld(fp, 7), "taken in this world: from it");
	Check(!RefFromWorld(fp, 8), "a load since: from the old world, gone");
	Check(!RefFromWorld(RefFingerprint{}, 7), "an empty fingerprint is from no world");
	Check(NextWorld(7, false) == 7, "a frame without a load: the same world");
	Check(NextWorld(7, true) == 8, "a frame while loading: a new world");
	Check(NextWorld(0xFFFFFFFFu, true) == 1, "the count wraps past 0, which no fingerprint carries");
}

// A read asked about at all: some bytes, not past the end of the address
// space.
void TestReadSpan() {
	std::printf("a span to read\n");
	Check(ReadSpanAsked(0x43000000u, 12), "twelve bytes of an object: asked");
	Check(!ReadSpanAsked(0x43000000u, 0), "no bytes: not asked");
	Check(ReadSpanAsked(0xFFFFFFF8u, 8), "ending at the address space's end: asked");
	Check(!ReadSpanAsked(0xFFFFFFF8u, 12), "wrapping the address space: not asked");
}

void TestReadablePages() {
	std::printf("readable pages remembered for one read\n");
	ReadablePages r;
	Check(!r.Covers(0x43000100u, 12), "nothing remembered: not covered");
	r.Remember(0x43000100u, 12);
	Check(r.Covers(0x43000000u, 1) && r.Covers(0x43000FF4u, 12), "anywhere on a remembered page: covered");
	Check(!r.Covers(0x43000FF8u, 12), "running onto the next page: not");
	Check(!r.Covers(0x42FFFFFCu, 8), "starting on the page before: not");
	r.Remember(0x43000FF8u, 12);
	Check(r.Covers(0x43000FF8u, 12) && r.Covers(0x43001800u, 4), "a span over two pages: both remembered");
	Check(!r.Covers(0x43000100u, 0), "no bytes: not covered");
	r.Remember(0x50000000u, 0);
	Check(!r.Covers(0x50000000u, 1), "no bytes are not remembered");
	r.Remember(0xFFFFFFF8u, 12);
	Check(!r.Covers(0xFFFFFFF8u, 12) && !r.Covers(0xFFFFFFF8u, 4), "a span wrapping the address space: not remembered, not covered");
	r.Remember(0xFFFFFFF8u, 8);
	Check(r.Covers(0xFFFFFFF0u, 16), "the last page, to the address space's end: covered");
	r.Remember(0x00000010u, 4);
	Check(r.Covers(0x00000020u, 4), "page 0 is a page like any other (the slots' empty mark is not page 0)");

	// one slot a page number's low bits: a page kSlots further takes it
	const UInt32 further = 0x43000000u + ReadablePages::kSlots * obvr::game::kPageBytes;
	r.Remember(further, 4);
	Check(r.Covers(further, 4), "a page on a taken slot: remembered");
	Check(!r.Covers(0x43000100u, 12), "the page that held the slot: given up");
	Check(r.Covers(0x43001800u, 4), "the pages on other slots stay");

	// a span of more pages than a read of an object's head ever is
	ReadablePages big;
	const UInt32 most = ReadablePages::kMostSpanPages;
	big.Remember(0x60000000u, (most + 1) * obvr::game::kPageBytes);
	Check(!big.Covers(0x60000000u, 1), "a span over more pages than kMostSpanPages: not remembered");
	for (UInt32 i = 0; i <= most; ++i) {
		big.Remember(0x60000000u + i * obvr::game::kPageBytes, 1);
	}
	Check(big.Covers(0x60000000u, most * obvr::game::kPageBytes), "kMostSpanPages pages, each remembered: covered");
	Check(!big.Covers(0x60000000u, (most + 1) * obvr::game::kPageBytes), "one page more: asked of the system again");
}

// What a probe asks of the system: SpanReadableBy with a reader that notes
// its calls and answers from a list of unreadable addresses.
struct Reader {
	UInt32 calls = 0;
	UInt32 lastAddress = 0;
	UInt32 lastBytes = 0;
	UInt32 bytesInAll = 0;
	UInt32 unreadableFrom = 0xFFFFFFFFu;  // a read reaching this address or beyond fails
	bool operator()(UInt32 address, UInt32 bytes) {
		++calls;
		lastAddress = address;
		lastBytes = bytes;
		if (address + bytes > unreadableFrom) {
			return false;
		}
		bytesInAll += bytes;
		return true;
	}
};

void TestSpanReadable() {
	std::printf("a span probed by reading it\n");
	const UInt32 chunk = obvr::game::kProbeChunkBytes;
	Reader a;
	Check(SpanReadableBy(a, 0x43000100u, 12) && a.calls == 1 && a.lastAddress == 0x43000100u && a.lastBytes == 12,
	      "an object's head: one read of exactly its bytes");
	Reader none;
	Check(!SpanReadableBy(none, 0x43000100u, 0) && none.calls == 0, "no bytes: not readable, the system not asked");
	Check(!SpanReadableBy(none, 0xFFFFFFF8u, 12) && none.calls == 0, "a wrapping span: not readable, the system not asked");
	Reader last;
	Check(SpanReadableBy(last, 0xFFFFFFF8u, 8) && last.calls == 1, "a span to the address space's end: read");
	Reader gone;
	gone.unreadableFrom = 0x43000000u;
	Check(!SpanReadableBy(gone, 0x43000100u, 12) && gone.calls == 1, "a read the system refuses: not readable");
	Reader longer;
	Check(SpanReadableBy(longer, 0x43000000u, 2 * chunk + 5) && longer.calls == 3 && longer.bytesInAll == 2 * chunk + 5 &&
	          longer.lastAddress == 0x43000000u + 2 * chunk && longer.lastBytes == 5,
	      "a span longer than a chunk: read chunk by chunk, every byte once");
	Reader exact;
	Check(SpanReadableBy(exact, 0x43000000u, chunk) && exact.calls == 1, "exactly a chunk: one read");
	Reader tail;
	tail.unreadableFrom = 0x43000000u + chunk + 1;
	Check(!SpanReadableBy(tail, 0x43000000u, 2 * chunk) && tail.calls == 2, "a later chunk refused: not readable, no chunk after it asked");
}

void TestSpanReadableWithPages() {
	std::printf("a span probed once a page\n");
	ReadablePages pages;
	Reader r;
	Check(SpanReadableBy(pages, r, 0x43000100u, 12) && r.calls == 1, "a page not yet known: the system asked");
	Check(SpanReadableBy(pages, r, 0x43000800u, 12) && r.calls == 1, "another object on the same page: not asked again");
	Check(SpanReadableBy(pages, r, 0x43000FF8u, 12) && r.calls == 2, "a span onto a page not yet known: asked");
	Check(SpanReadableBy(pages, r, 0x43001004u, 1) && r.calls == 2, "that page is known from then on");
	Reader gone;
	gone.unreadableFrom = 0x50000000u;
	Check(!SpanReadableBy(pages, gone, 0x50000100u, 12) && gone.calls == 1, "a refused read: not readable");
	Check(!SpanReadableBy(pages, gone, 0x50000100u, 12) && gone.calls == 2, "and not remembered: asked again");
	Check(!SpanReadableBy(pages, gone, 0x50000100u, 0) && gone.calls == 2, "no bytes: not readable, the system not asked");
}

}  // namespace

int main() {
	TestStillThere();
	TestGone();
	TestEmptyFingerprint();
	TestGameImage();
	TestLooksLive();
	TestWorld();
	TestReadSpan();
	TestReadablePages();
	TestSpanReadable();
	TestSpanReadableWithPages();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
