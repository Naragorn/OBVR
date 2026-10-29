// Checks the log archive's decisions (core/LogArchiveLogic.h): the copy's
// name, which files count as the archive's, and which go when it is full.

#include <cstdio>
#include <string>
#include <vector>

#include "core/LogArchiveLogic.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::log::ArchiveName;
using obvr::log::ArchivesToRemove;
using obvr::log::IsArchiveName;

void TestName() {
	std::printf("The copy's name\n");
	Check(ArchiveName(2026, 10, 29, 12, 49, 3) == "OBVR-2026-10-29_12-49-03.log", "date and time, zero-padded");
	Check(ArchiveName(2026, 9, 1, 8, 5, 0) < ArchiveName(2026, 10, 29, 12, 49, 3), "sorts by time");
	Check(ArchiveName(2026, 10, 29, 9, 0, 0) < ArchiveName(2026, 10, 29, 10, 0, 0), "and within a day");
	// FILETIMEs from Unix times read with `date -u -d ... +%s` (1793278143, 1709251199).
	Check(obvr::log::ArchiveNameFor(134377517430000000ull) == "OBVR-2026-10-29_12-49-03.log",
	      "a FILETIME: 2026-10-29 12:49:03");
	Check(obvr::log::ArchiveNameFor(133537247990000000ull) == "OBVR-2024-02-29_23-59-59.log",
	      "a leap day's last second");
	Check(obvr::log::ArchiveNameFor(133537248000000000ull) == "OBVR-2024-03-01_00-00-00.log", "and the next");
	Check(obvr::log::ArchiveNameFor(116444736000000000ull) == "OBVR-1970-01-01_00-00-00.log", "the Unix epoch");
}

void TestIsArchive() {
	std::printf("What counts as the archive's\n");
	Check(IsArchiveName("OBVR-2026-10-29_12-49-03.log"), "a copy's name");
	Check(!IsArchiveName("OBVR.log"), "not the live log");
	Check(!IsArchiveName("OBVR.log.prev"), "not the previous log");
	Check(!IsArchiveName("notes.txt"), "not anything else");
	Check(!IsArchiveName("OBVR-2026-10-29_12-49-03.txt"), "not another extension");
	Check(!IsArchiveName("OBVR-2026-10-29-12-49-03.log"), "not a wrong separator");
	Check(!IsArchiveName("OBVR-2026-1a-29_12-49-03.log"), "not a letter in a number");
	Check(!IsArchiveName("OBVR-2026-10-29_12-49-030.log"), "not a longer name");
}

void TestRemove() {
	std::printf("Which go when it is full\n");
	const std::vector<std::string> three = {"OBVR-2026-10-29_12-00-00.log", "OBVR-2026-10-28_09-00-00.log",
	                                        "OBVR-2026-10-29_08-00-00.log"};
	Check(ArchivesToRemove(three, 3).empty(), "three kept of three: none");
	Check(ArchivesToRemove(three, 5).empty(), "under the count: none");
	const std::vector<std::string> gone = ArchivesToRemove(three, 2);
	Check(gone.size() == 1 && gone[0] == "OBVR-2026-10-28_09-00-00.log", "one over: the oldest");
	const std::vector<std::string> two = ArchivesToRemove(three, 1);
	Check(two.size() == 2 && two[0] == "OBVR-2026-10-28_09-00-00.log" && two[1] == "OBVR-2026-10-29_08-00-00.log",
	      "two over: the two oldest");
	std::vector<std::string> mixed = three;
	mixed.push_back("readme.txt");
	mixed.push_back("OBVR.log");
	const std::vector<std::string> mixedGone = ArchivesToRemove(mixed, 2);
	Check(mixedGone.size() == 1 && mixedGone[0] == "OBVR-2026-10-28_09-00-00.log",
	      "other files neither count nor go");
	Check(ArchivesToRemove({}, 3).empty(), "an empty folder: none");
	Check(ArchivesToRemove(three, 0).size() == 3, "keep 0: all of the archive's");
}

}  // namespace

int main() {
	std::printf("OBVR log archive test\n\n");
	TestName();
	TestIsArchive();
	TestRemove();
	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}
	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
