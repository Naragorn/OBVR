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
using obvr::game::MemoryRangeReadable;
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

// Whether the bytes a check reads lie in committed, readable memory - read off
// VirtualQuery's region around the address.
void TestRangeReadable() {
	std::printf("readable memory\n");
	constexpr UInt32 kCommit = 0x1000;
	constexpr UInt32 kReserve = 0x2000;
	constexpr UInt32 kFree = 0x10000;
	constexpr UInt32 base = 0x43000000;
	constexpr UInt32 size = 0x00100000;
	Check(MemoryRangeReadable(base, size, kCommit, 0x04, base + 0x6E948, 12), "committed read-write: readable");
	Check(MemoryRangeReadable(base, size, kCommit, 0x02, base, 12), "read-only: readable");
	Check(MemoryRangeReadable(base, size, kCommit, 0x20, base, 12), "execute-read: readable");
	Check(MemoryRangeReadable(base, size, kCommit, 0x40, base, 12), "execute-read-write: readable");
	Check(!MemoryRangeReadable(base, size, kReserve, 0x04, base, 12), "reserved, not committed (a heap given back): not");
	Check(!MemoryRangeReadable(base, size, kFree, 0x01, base, 12), "free: not");
	Check(!MemoryRangeReadable(base, size, kCommit, 0x01, base, 12), "no access: not");
	Check(!MemoryRangeReadable(base, size, kCommit, 0x10, base, 12), "execute only: not");
	Check(!MemoryRangeReadable(base, size, kCommit, 0x04 | 0x100, base, 12), "a guard page: not");
	Check(!MemoryRangeReadable(base, size, kCommit, 0x04, base + size - 8, 12), "running past the region's end: not");
	Check(MemoryRangeReadable(base, size, kCommit, 0x04, base + size - 12, 12), "ending at the region's end: readable");
	Check(!MemoryRangeReadable(base, size, kCommit, 0x04, base - 4, 12), "starting before the region: not");
	Check(!MemoryRangeReadable(base, 0, kCommit, 0x04, base, 12), "an empty region: not");
	Check(!MemoryRangeReadable(0xFFFFFFF0u, 0x10, kCommit, 0x04, 0xFFFFFFF8u, 12), "a range wrapping the address space: not");
}

}  // namespace

int main() {
	TestStillThere();
	TestGone();
	TestEmptyFingerprint();
	TestGameImage();
	TestLooksLive();
	TestWorld();
	TestRangeReadable();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
