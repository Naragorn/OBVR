// Checks the "HandScript: actors" line (game/ActorRosterLogic.h): every flow
// of the grouping by base form, the dead counts, the overflow past the named
// bases, the nearest actor and the cut to the buffer.

#include <cstdio>
#include <cstring>
#include <string>

#include "game/ActorRosterLogic.h"

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

std::string Line(const RosterActor* actors, UInt32 count, size_t size = 512) {
	char buf[512];
	FormatRosterLine(actors, count, buf, size < sizeof(buf) ? size : sizeof(buf));
	return buf;
}

void TestNearest() {
	std::printf("The nearest actor\n");
	Check(NearestRosterActor(nullptr, 0) == -1, "nobody: -1");
	const RosterActor three[] = {{0x1, 0xA, false, 300.0f}, {0x2, 0xA, true, 20.0f}, {0x3, 0xB, false, 20.0f}};
	Check(NearestRosterActor(three, 3) == 1, "the smallest distance, the first of a tie, dead or not");
	Check(NearestRosterActor(three, 1) == 0, "one actor: that one");
}

void TestLine() {
	std::printf("The line's text\n");
	Check(Line(nullptr, 0) == "0 loaded, 0 dead: none", "nobody loaded");

	const RosterActor one[] = {{0xFF000A3B, 0x00069AD1, false, 120.4f}};
	Check(Line(one, 1) == "1 loaded, 0 dead: 00069AD1 x1; nearest FF000A3B at 120 units",
	      "one alive: its base once, nearest without a dead note");

	const RosterActor dead[] = {{0xFF000A3B, 0x00069AD1, true, 120.4f}};
	Check(Line(dead, 1) == "1 loaded, 1 dead: 00069AD1 x1 (dead 1); nearest FF000A3B at 120 units (dead)",
	      "one dead: counted in the base and in the total, the nearest marked dead");

	const RosterActor mixed[] = {{0x10, 0x00069AD1, false, 500.0f},
	                             {0x11, 0x00025E6E, false, 90.0f},
	                             {0x12, 0x00069AD1, true, 700.0f},
	                             {0x13, 0x00069AD1, false, 800.0f}};
	Check(Line(mixed, 4) == "4 loaded, 1 dead: 00069AD1 x3 (dead 1), 00025E6E x1; nearest 00000011 at 90 units",
	      "bases in first-seen order, alive and dead summed per base, the dead noted");

	RosterActor many[kRosterGroupsMost + 3];
	for (UInt32 i = 0; i < kRosterGroupsMost + 3; ++i) {
		many[i] = RosterActor{0x100 + i, 0x1000 + i, false, 10.0f + static_cast<float>(i)};
	}
	const std::string line = Line(many, kRosterGroupsMost + 3);
	Check(line.find(" and 3 more") != std::string::npos, "past the named bases: 'and N more'");
	Check(line.rfind("27 loaded, 0 dead: 00001000 x1, ", 0) == 0, "the first bases still named");
	Check(line.find("; nearest 00000100 at 10 units") != std::string::npos, "the nearest found among all of them");

	const std::string cut = Line(mixed, 4, 24);
	Check(cut.size() == 23 && cut == std::string("4 loaded, 1 dead: 00069AD1 x3 (dead 1), 00025E6E x1").substr(0, 23),
	      "a small buffer: cut and NUL-terminated");
}

}  // namespace

int main() {
	TestNearest();
	TestLine();
	if (g_failures > 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
