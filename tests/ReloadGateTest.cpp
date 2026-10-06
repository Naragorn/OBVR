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

}  // namespace

int main() {
	TestNothingRemembered();
	TestUnreadable();
	TestAfterLoad();
	TestIniChanges();
	TestOverlay();
	TestStampEquality();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
