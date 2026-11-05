// Checks Walkers' 'WGCT' (game/CampKillLogic.h): a walker a camp's refugee
// killed, read from the message and kept until the frame cuts its head.

#include <cstdio>

#include "game/CampKillLogic.h"

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

void TestParse() {
	std::printf("Reading 'WGCT'\n");
	Check(kWalkersCampKillMessage == 0x54434757u, "the type 'WGCT'");
	const UInt32 ref = 0xFF000F9Cu;
	UInt32 out = 0;
	Check(ParseCampKill(&ref, 4, out) && out == ref, "4 bytes: the walker's reference");
	Check(!ParseCampKill(&ref, 8, out), "another size: refused");
	Check(!ParseCampKill(nullptr, 4, out), "no data: refused");
	const UInt32 none = 0;
	Check(!ParseCampKill(&none, 4, out), "reference 0: refused");
}

void TestQueue() {
	std::printf("Kept until the frame takes them\n");
	CampKillQueue q;
	Check(TakeCampKill(q) == 0, "empty: nothing");
	Check(PushCampKill(q, 0x11) && PushCampKill(q, 0x22), "two kills kept");
	Check(PushCampKill(q, 0x11) && q.count == 2, "the same walker again: kept once");
	Check(TakeCampKill(q) == 0x11 && TakeCampKill(q) == 0x22 && TakeCampKill(q) == 0, "taken in order, then none");
	for (UInt32 i = 1; i <= kCampKillsKept; ++i) {
		PushCampKill(q, i);
	}
	Check(!PushCampKill(q, 0x99) && q.count == kCampKillsKept, "full: the next refused, the kept ones stay");
}

}  // namespace

int main() {
	TestParse();
	TestQueue();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
