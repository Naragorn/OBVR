// Checks the fixed-capacity pointer set that remembers which vertex
// buffers were ever locked with DISCARD. The draw hooks consult it on
// every draw, so its promises are small but load-bearing: membership is
// exact, a duplicate insert never grows it, and a full set refuses new
// pointers instead of overwriting old ones - losing a known buffer would
// silently declassify every skinned draw wired to it.

#include <cstdio>

#include "render/PointerSet.h"

namespace {

using obvr::render::PointerSet;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

// Distinct addresses to fill the set with, without touching real memory.
void* Fake(UInt32 n) {
	return reinterpret_cast<void*>(static_cast<UInt32>(0x1000 + n * 0x10));
}

void TestMembership() {
	std::printf("Membership\n");

	PointerSet set;
	Check(!set.Contains(Fake(1)), "an empty set contains nothing");

	Check(set.Insert(Fake(1)), "a first insert is accepted");
	Check(set.Contains(Fake(1)), "and the pointer is a member afterwards");
	Check(!set.Contains(Fake(2)), "other pointers are still not members");
	Check(set.count == 1, "one insert, one entry");

	Check(set.Insert(Fake(1)), "inserting a member again is accepted");
	Check(set.count == 1, "but does not grow the set");
}

void TestCapacity() {
	std::printf("Capacity\n");

	PointerSet set;
	bool allAccepted = true;
	for (UInt32 i = 0; i < PointerSet::kCapacity; ++i) {
		allAccepted = set.Insert(Fake(i)) && allAccepted;
	}
	Check(allAccepted, "the set accepts exactly its capacity");
	Check(set.count == PointerSet::kCapacity, "and holds that many");

	Check(!set.Insert(Fake(PointerSet::kCapacity)), "one more new pointer is refused");
	Check(set.count == PointerSet::kCapacity, "and the set did not grow");
	Check(!set.Contains(Fake(PointerSet::kCapacity)), "the refused pointer is not a member");

	Check(set.Insert(Fake(0)), "a full set still accepts its own members");
	Check(set.Contains(Fake(0)) && set.Contains(Fake(PointerSet::kCapacity - 1)),
	      "first and last member survive the refusals");
}

}  // namespace

int main() {
	std::printf("OBVR pointer set test\n\n");

	TestMembership();
	std::printf("\n");
	TestCapacity();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
