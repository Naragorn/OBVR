// Exercises the walk that rewrites Oblivion's in-memory screen size, over
// setting lists this test builds itself. The refusal flows are the point:
// this code writes into engine memory, so every way it can decline to has to
// be seen declining.

#include <cstdio>

#include "game/IniSettings.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s: %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using obvr::game::IniSettingEntry;
using obvr::game::IniSettingInfo;
using obvr::game::OverrideSizeSettings;

// A three-entry list shaped like the engine's: width, an unrelated setting,
// height. Rebuilt fresh for every flow so no test sees another's writes.
struct SampleList {
	IniSettingInfo width;
	IniSettingInfo unrelated;
	IniSettingInfo height;
	IniSettingEntry entries[3];

	SampleList(int currentWidth, int currentHeight) {
		width.i = currentWidth;
		width.name = "iSize W";
		unrelated.i = 7;
		unrelated.name = "iAdapter";
		height.i = currentHeight;
		height.name = "iSize H";
		entries[0] = {&width, &entries[1]};
		entries[1] = {&unrelated, &entries[2]};
		entries[2] = {&height, nullptr};
	}
};

void TestRewrite() {
	std::printf("When the list is what it claims to be\n");

	SampleList list(2560, 1440);
	Check(OverrideSizeSettings(&list.entries[0], 2560, 1440, 4028, 3380),
	      "matching names and values are rewritten");
	Check(list.width.i == 4028 && list.height.i == 3380, "to the new size, both of them");
	Check(list.unrelated.i == 7, "and the unrelated neighbour is untouched");

	// The way back, which the hook takes when CreateDevice refuses its
	// parameters: the same walk with the numbers swapped.
	Check(OverrideSizeSettings(&list.entries[0], 4028, 3380, 2560, 1440),
	      "the rewrite can be undone by the same walk");
	Check(list.width.i == 2560 && list.height.i == 1440, "restoring the game's own size");
}

void TestValueMismatch() {
	std::printf("When the current values are not what the game asked for\n");

	{
		SampleList list(1920, 1440);
		Check(!OverrideSizeSettings(&list.entries[0], 2560, 1440, 4028, 3380),
		      "a wrong width refuses");
		Check(list.width.i == 1920 && list.height.i == 1440, "and writes nothing");
	}
	{
		SampleList list(2560, 1080);
		Check(!OverrideSizeSettings(&list.entries[0], 2560, 1440, 4028, 3380),
		      "a wrong height refuses");
		Check(list.width.i == 2560 && list.height.i == 1080,
		      "and the width is not written alone - both or neither");
	}
}

void TestMissingSettings() {
	std::printf("When a setting is not in the list\n");

	{
		SampleList list(2560, 1440);
		list.width.name = "iSomethingElse";
		Check(!OverrideSizeSettings(&list.entries[0], 2560, 1440, 4028, 3380),
		      "a missing width refuses");
		Check(list.height.i == 1440, "and the height alone is not written");
	}
	{
		SampleList list(2560, 1440);
		// Case matters: this walk writes on the strength of the comparison,
		// and the engine's own spelling is fixed in the binary.
		list.width.name = "isize w";
		Check(!OverrideSizeSettings(&list.entries[0], 2560, 1440, 4028, 3380),
		      "a name in the wrong case is not the setting");
	}
	Check(!OverrideSizeSettings(nullptr, 2560, 1440, 4028, 3380), "an empty list refuses");
}

void TestRaggedList() {
	std::printf("When the list is ragged or broken\n");

	// Entries with no data or no name are the engine's business, not a fault:
	// the walk steps over them and still finds what it came for.
	{
		SampleList list(2560, 1440);
		IniSettingInfo nameless{};
		nameless.name = nullptr;
		IniSettingEntry hole{nullptr, &list.entries[0]};
		IniSettingEntry blank{&nameless, &hole};
		Check(OverrideSizeSettings(&blank, 2560, 1440, 4028, 3380),
		      "dataless and nameless entries are stepped over");
		Check(list.width.i == 4028, "and the settings behind them are still found");
	}

	// A cycle must end in a refusal, not a hang: the walk is bounded.
	{
		SampleList list(2560, 1440);
		IniSettingInfo decoy{};
		decoy.i = 1;
		decoy.name = "iDecoy";
		IniSettingEntry loop{&decoy, nullptr};
		loop.next = &loop;
		Check(!OverrideSizeSettings(&loop, 2560, 1440, 4028, 3380),
		      "a cyclic list ends in a refusal rather than a hang");
	}
}

}  // namespace

int main() {
	std::printf("OBVR ini settings test\n\n");

	TestRewrite();
	std::printf("\n");
	TestValueMismatch();
	std::printf("\n");
	TestMissingSettings();
	std::printf("\n");
	TestRaggedList();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
