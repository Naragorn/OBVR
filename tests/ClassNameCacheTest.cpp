#include <cstdio>

#include "game/ClassNameCacheLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::game::VtableNames;

void TestNames() {
	std::printf("the class names kept by vtable\n");
	VtableNames names;
	static const char kNode[] = "NiNode@@";
	static const char kShape[] = "NiTriShape@@";
	Check(names.Find(0x00A7E38Cu) == nullptr, "nothing kept: not found");
	names.Remember(0x00A7E38Cu, kNode);
	Check(names.Find(0x00A7E38Cu) == kNode, "a kept vtable: its name, the same pointer");
	Check(names.Find(0x00A7E390u) == nullptr, "the next vtable word: not found");
	names.Remember(0x00A7F27Cu, kShape);
	Check(names.Find(0x00A7F27Cu) == kShape && names.Find(0x00A7E38Cu) == kNode, "a second one beside the first");
	names.Remember(0, kNode);
	Check(names.Find(0) == nullptr, "no vtable is never kept (an empty slot is not an answer)");
	names.Remember(0x00A80000u, nullptr);
	Check(names.Find(0x00A80000u) == nullptr, "no name is never kept");
	names.Remember(0x00A7E38Cu, kShape);
	Check(names.Find(0x00A7E38Cu) == kShape, "kept again: the later name");
}

void TestSlots() {
	std::printf("one slot a vtable, a later one taking it\n");
	VtableNames names;
	static const char kFirst[] = "First@@";
	static const char kSecond[] = "Second@@";
	// two vtables on one slot: found by looking, so the test does not know the hash
	const UInt32 a = 0x00A70000u;
	UInt32 b = 0;
	for (UInt32 candidate = a + 4; candidate < a + 0x100000u; candidate += 4) {
		if (VtableNames::SlotOf(candidate) == VtableNames::SlotOf(a)) {
			b = candidate;
			break;
		}
	}
	Check(b != 0, "another vtable on the same slot exists");
	names.Remember(a, kFirst);
	names.Remember(b, kSecond);
	Check(names.Find(b) == kSecond, "the later vtable holds the slot");
	Check(names.Find(a) == nullptr, "the earlier one is asked afresh, never answered with the other's name");
	UInt32 used = 0;
	for (UInt32 v = 0x00A70000u; v < 0x00A70000u + 4 * 64; v += 4) {
		used |= 1u << (VtableNames::SlotOf(v) % 32);
	}
	Check(used == 0xFFFFFFFFu, "neighbouring vtables spread over the slots");
	Check(VtableNames::SlotOf(0xFFFFFFFCu) < VtableNames::kSlots, "the highest address still lands in a slot");
}

}  // namespace

int main() {
	TestNames();
	TestSlots();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
