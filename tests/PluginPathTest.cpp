// Checks the path building that decides where OBVR looks for its own files.
//
// This is short code with an outsized reach: it picks between the plugin
// directory, which Mod Organizer 2 virtualises, and the game root, which it
// does not. It also writes into a caller-supplied buffer, so the case where
// that buffer is too small is worth pinning down - a path is one of the few
// things in OBVR whose length is not under its own control.
//
// Windows only, since GetModuleFileNameA is what both anchors are built on.
//
// Not covered here: core/Memory. Verify and SafeWrite take an address as a
// UInt32 and cast it straight to a pointer, which is correct for the 32-bit
// process OBVR is loaded into but truncates in a native 64-bit test build.
// They are only exercisable from a 32-bit build, and the trampoline test
// already covers the byte generation those functions transport.

#include <cstdio>
#include <cstring>

#include "platform/PluginPath.h"
#include "platform/Win32Min.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool EndsWith(const char* text, const char* suffix) {
	const size_t textLength = std::strlen(text);
	const size_t suffixLength = std::strlen(suffix);
	if (suffixLength > textLength) {
		return false;
	}
	return std::strcmp(text + textLength - suffixLength, suffix) == 0;
}

void TestPluginAnchorNeedsTheModule() {
	std::printf("Plugin anchor before the module handle is known\n");

	// DllMain hands OBVR its module handle. Until that has happened there is
	// no plugin directory to speak of, and the caller has to fall back rather
	// than receive a path built from nothing. Config depends on exactly this
	// to reach the game root.
	char path[512];
	Check(!obvr::platform::BuildPluginPath("OBVR.ini", path, sizeof(path)),
	      "without a module handle the plugin anchor declines");
}

void TestGameAnchor() {
	std::printf("Game anchor\n");

	char path[512];
	Check(obvr::platform::BuildGamePath("OBVR.ini", path, sizeof(path)),
	      "the game anchor works without any handle being set");
	Check(EndsWith(path, "\\OBVR.ini"), "the result ends in the requested file name");
	Check(path[1] == ':', "the result is an absolute path");

	// The file name is replaced, not appended: asking twice must not stack up.
	char again[512];
	obvr::platform::BuildGamePath("OBVR.log", again, sizeof(again));
	Check(EndsWith(again, "\\OBVR.log"), "a second call replaces the file name");
	Check(!EndsWith(again, "OBVR.ini\\OBVR.log"), "and does not append to the previous one");
}

void TestBufferTooSmall() {
	std::printf("Buffer too small\n");

	// The directory alone will not fit in these, let alone the file name. The
	// contract is a plain false; anything written past the end would be a
	// stack overflow in the game process.
	char tiny[8];
	Check(!obvr::platform::BuildGamePath("OBVR.ini", tiny, sizeof(tiny)),
	      "a buffer far too small is refused");

	// A buffer that holds the directory but not the file name has to be
	// refused as well. This is the interesting one: the first step succeeds
	// and only the second runs out of room.
	char path[512];
	obvr::platform::BuildGamePath("", path, sizeof(path));
	const size_t directoryLength = std::strlen(path);

	char justShort[512];
	const UInt32 limit = static_cast<UInt32>(directoryLength) + 4;
	Check(!obvr::platform::BuildGamePath("OBVR.ini", justShort, limit),
	      "a buffer that fits the directory but not the file name is refused");
}

void TestPluginAnchorOnceKnown() {
	std::printf("Plugin anchor once the module handle is known\n");

	// A test binary has no OBVR.dll to point at, so its own module stands in.
	// That makes the plugin anchor resolve to the same directory as the game
	// anchor, which is enough to show the handle is actually used and that the
	// same file name replacement happens.
	obvr::platform::SetPluginModule(GetModuleHandleA(nullptr));

	char plugin[512];
	char game[512];
	Check(obvr::platform::BuildPluginPath("OBVR.ini", plugin, sizeof(plugin)),
	      "with a handle set the plugin anchor works");
	obvr::platform::BuildGamePath("OBVR.ini", game, sizeof(game));

	Check(std::strcmp(plugin, game) == 0,
	      "pointed at the running module, both anchors agree");
	Check(EndsWith(plugin, "\\OBVR.ini"), "the plugin anchor also ends in the file name");
}

}  // namespace

int main() {
	std::printf("OBVR plugin path test\n\n");

	// Order matters: the first case relies on no module handle having been set
	// yet, and the last one sets it.
	TestPluginAnchorNeedsTheModule();
	std::printf("\n");
	TestGameAnchor();
	std::printf("\n");
	TestBufferTooSmall();
	std::printf("\n");
	TestPluginAnchorOnceKnown();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
