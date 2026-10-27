// Checks the co-save record of the four places (game/HolsterSlotRecord.h): read
// back with the ids resolved, refused for another length or version, an id
// that does not resolve left empty, and one form at one place at a time.

#include <cstdio>

#include "game/HolsterSlotRecord.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

constexpr UInt32 kHipL = static_cast<UInt32>(vr::HolsterSlot::HipLeft);
constexpr UInt32 kBackR = static_cast<UInt32>(vr::HolsterSlot::BackRight);

}  // namespace

int main() {
	std::printf("The places' record\n");
	HolsterSlotsRecord record;
	record.ids[kHipL] = 0x00000C0C;
	record.ids[kBackR] = 0x01001234;
	UInt32 out[vr::kHolsterSlotCount];
	const auto same = [](UInt32 id, UInt32& to) {
		to = id;
		return true;
	};
	Check(HolsterSlotsOfRecord(&record, sizeof(record), kHolsterSlotsVersion, same, out) && out[kHipL] == 0x00000C0C &&
	          out[kBackR] == 0x01001234 && out[0] == 0,
	      "read back as written");
	const auto moved = [](UInt32 id, UInt32& to) {
		if ((id >> 24) == 0x01) {
			to = (id & 0x00FFFFFF) | 0x03000000;
			return true;
		}
		to = id;
		return true;
	};
	HolsterSlotsOfRecord(&record, sizeof(record), kHolsterSlotsVersion, moved, out);
	Check(out[kBackR] == 0x03001234, "a plugin moved in the load order: the id follows it");
	const auto gone = [](UInt32 id, UInt32& to) {
		to = id;
		return (id >> 24) == 0x00;
	};
	HolsterSlotsOfRecord(&record, sizeof(record), kHolsterSlotsVersion, gone, out);
	Check(out[kHipL] == 0x00000C0C && out[kBackR] == 0, "a plugin gone: that place empty, the rest kept");
	Check(!HolsterSlotsOfRecord(&record, sizeof(record) - 4, kHolsterSlotsVersion, same, out) && out[kHipL] == 0,
	      "another length: refused, all empty");
	Check(!HolsterSlotsOfRecord(&record, sizeof(record), kHolsterSlotsVersion + 1, same, out), "another version: refused");
	Check(!HolsterSlotsOfRecord(nullptr, sizeof(record), kHolsterSlotsVersion, same, out), "no data: refused");

	std::printf("One form at one place\n");
	UInt32 ids[vr::kHolsterSlotCount] = {};
	KeepAtSlot(ids, vr::HolsterSlot::HipLeft, 0x10);
	KeepAtSlot(ids, vr::HolsterSlot::HipRight, 0x10);
	Check(ids[kHipL] == 0 && ids[static_cast<UInt32>(vr::HolsterSlot::HipRight)] == 0x10, "kept elsewhere: moved");
	KeepAtSlot(ids, vr::HolsterSlot::None, 0x20);
	KeepAtSlot(ids, vr::HolsterSlot::BackLeft, 0);
	Check(ids[0] == 0 && ids[static_cast<UInt32>(vr::HolsterSlot::BackLeft)] == 0, "no place, or no form: nothing kept");
	Check(SlotKeeping(ids, 0x10) == vr::HolsterSlot::HipRight && SlotKeeping(ids, 0x99) == vr::HolsterSlot::None &&
	          SlotKeeping(ids, 0) == vr::HolsterSlot::None,
	      "which place keeps a form");
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
