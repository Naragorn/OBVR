// Checks the hot reload's decision: a file that cannot be asked keeps what is
// loaded, the same files read nothing, any change in either file reads once
// and is then remembered, and a Load's memory spares the first reload.

#include <cstdio>

#include "core/ReloadGate.h"

namespace {

using obvr::config::Decide;
using obvr::config::FileStamp;
using obvr::config::ReloadGate;
using obvr::config::ReloadVerdict;
using obvr::config::Remember;
using obvr::config::RememberOwnWrite;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

FileStamp Ini(UInt64 write, UInt64 size) {
	FileStamp s;
	s.iniKnown = true;
	s.iniWrite = write;
	s.iniSize = size;
	return s;
}

void TestNothingRemembered() {
	std::printf("Nothing remembered\n");

	ReloadGate gate;
	Check(Decide(gate, Ini(100, 5000)) == ReloadVerdict::Changed,
	      "a file seen for the first time is read");
	Check(Decide(gate, Ini(100, 5000)) == ReloadVerdict::Unchanged,
	      "and the second look at it reads nothing");
}

void TestUnreadable() {
	std::printf("Unreadable\n");

	ReloadGate gate;
	FileStamp missing;
	Check(Decide(gate, missing) == ReloadVerdict::Unreadable,
	      "a file that cannot be asked is not read");
	Check(!gate.seen.iniKnown, "and nothing is remembered of it");

	Remember(gate, Ini(100, 5000));
	Check(Decide(gate, missing) == ReloadVerdict::Unreadable,
	      "the same with a file remembered: the loaded values stay");
	Check(gate.seen == Ini(100, 5000), "the memory is untouched");
	Check(Decide(gate, Ini(100, 5000)) == ReloadVerdict::Unchanged,
	      "the file coming back as it was reads nothing");
	Check(Decide(gate, Ini(200, 5000)) == ReloadVerdict::Changed,
	      "coming back changed, it is read");
}

void TestAfterLoad() {
	std::printf("After Load\n");

	ReloadGate gate;
	Remember(gate, Ini(100, 5000));
	Check(Decide(gate, Ini(100, 5000)) == ReloadVerdict::Unchanged,
	      "the first reload after Load reads nothing");
	Check(Decide(gate, Ini(100, 5000)) == ReloadVerdict::Unchanged,
	      "nor the hundredth");
}

void TestIniChanges() {
	std::printf("OBVR.ini changes\n");

	ReloadGate gate;
	Remember(gate, Ini(100, 5000));
	Check(Decide(gate, Ini(101, 5000)) == ReloadVerdict::Changed,
	      "a later write time is read");
	Check(Decide(gate, Ini(101, 5000)) == ReloadVerdict::Unchanged,
	      "and remembered");
	Check(Decide(gate, Ini(101, 5001)) == ReloadVerdict::Changed,
	      "a changed size within the same instant is read too");
	Check(Decide(gate, Ini(50, 5001)) == ReloadVerdict::Changed,
	      "an earlier write time - a restored copy - is a change as well");
}

void TestOverlay() {
	std::printf("OBVR-test.ini\n");

	ReloadGate gate;
	Remember(gate, Ini(100, 5000));

	FileStamp withOverlay = Ini(100, 5000);
	withOverlay.overlayWrite = 300;
	withOverlay.overlaySize = 40;
	Check(Decide(gate, withOverlay) == ReloadVerdict::Changed,
	      "a test overlay appearing is read");
	Check(Decide(gate, withOverlay) == ReloadVerdict::Unchanged,
	      "and remembered");

	FileStamp rewritten = withOverlay;
	rewritten.overlayWrite = 301;
	Check(Decide(gate, rewritten) == ReloadVerdict::Changed,
	      "the overlay rewritten is read");

	Check(Decide(gate, Ini(100, 5000)) == ReloadVerdict::Changed,
	      "the overlay gone is read, so the player's values come back");
	Check(Decide(gate, Ini(100, 5000)) == ReloadVerdict::Unchanged,
	      "and that is remembered too");
}

void TestStampEquality() {
	std::printf("Stamp equality\n");

	FileStamp a = Ini(1, 2);
	FileStamp b = Ini(1, 2);
	Check(a == b, "equal fields are equal");
	b.overlaySize = 1;
	Check(a != b, "an overlay size apart is not");
	FileStamp missing;
	Check(missing != a, "a missing file is not a present one");
	Check(missing == FileStamp{}, "two missing files are the same state");
}

// OBVR's own write (SaveSetting): the value is in memory already, so the file
// it changed is remembered as seen and the next check reads nothing - unless
// something else changed the files before the write and that change was not
// read yet: then the next check still reads, so that change is not lost.
// (Nadi's Windows run of 2026-10-27: five settings writes, each followed by a
// re-read of 90 to 105 ms.)
void TestOwnWrite() {
	std::printf("OBVR's own write\n");

	ReloadGate gate;
	Remember(gate, Ini(100, 5000));
	Check(RememberOwnWrite(gate, Ini(100, 5000), Ini(200, 5004)), "nothing pending: the write is remembered");
	Check(Decide(gate, Ini(200, 5004)) == ReloadVerdict::Unchanged, "so the next check reads nothing");
	Check(RememberOwnWrite(gate, Ini(200, 5004), Ini(210, 5004)), "a second write right after: remembered too");
	Check(Decide(gate, Ini(210, 5004)) == ReloadVerdict::Unchanged, "still nothing to read");

	ReloadGate pending;
	Remember(pending, Ini(100, 5000));
	// Somebody edited the file (150) and the gate has not looked since.
	Check(!RememberOwnWrite(pending, Ini(150, 5100), Ini(200, 5104)),
	      "an outside change not read yet: the write is not remembered");
	Check(Decide(pending, Ini(200, 5104)) == ReloadVerdict::Changed, "so the next check reads it, both changes");

	ReloadGate never;
	Check(!RememberOwnWrite(never, Ini(100, 5000), Ini(200, 5004)), "nothing ever read: not remembered");
	Check(Decide(never, Ini(200, 5004)) == ReloadVerdict::Changed, "the first check reads");

	ReloadGate unknown;
	Remember(unknown, Ini(100, 5000));
	Check(!RememberOwnWrite(unknown, FileStamp{}, Ini(200, 5004)), "the file unreadable before the write: not");
	Check(!RememberOwnWrite(unknown, Ini(100, 5000), FileStamp{}), "unreadable after it: not");
	Check(Decide(unknown, Ini(200, 5004)) == ReloadVerdict::Changed, "and the change is read at the next check");
}

}  // namespace

int main() {
	TestNothingRemembered();
	TestUnreadable();
	TestAfterLoad();
	TestIniChanges();
	TestOverlay();
	TestStampEquality();
	TestOwnWrite();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
