// Checks the table that connects a menu row to the setting behind it.
//
// A table of getters and setters is written by copying the row above and
// editing it, and there is one way that goes wrong which nothing else catches:
// a row that reads its own field and writes the field it was copied from. It
// behaves perfectly - the menu shows the right value, the arrow keys move it -
// right up until somebody changes that setting, and then it quietly changes a
// different one. Nobody finds that by reading the table, because the table
// reads correctly; the eye supplies the name it expects.
//
// So the last test here writes through every single row and then checks that
// every other row still reads what it read before. That is the check the eye
// cannot do, and it is the reason this file exists.
//
// No Windows API here, so this one builds and runs on Linux as well.

#include <cstdio>

#include "ui/SettingsList.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool TextIs(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return a == b;
	}
	while (*a != '\0' && *b != '\0') {
		if (*a != *b) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

using obvr::Config;
using obvr::ui::ApplySetting;
using obvr::ui::ItemFor;
using obvr::ui::ItemKind;
using obvr::ui::SettingDefinition;
using obvr::ui::SettingDefinitionCount;
using obvr::ui::SettingDefinitions;

// A value the given row is definitely not sitting at, so that a write through
// it is a change rather than a no-op - a no-op would make the isolation test
// below pass for the wrong reason.
float DifferentValue(const SettingDefinition& definition, float current) {
	if (definition.kind == ItemKind::Toggle) {
		return current != 0.0f ? 0.0f : 1.0f;
	}
	return current != definition.maximum ? definition.maximum : definition.minimum;
}

void TestTableIsComplete() {
	std::printf("Every row is filled in\n");

	const SettingDefinition* const settings = SettingDefinitions();
	const UInt32 count = SettingDefinitionCount();

	Check(count > 0, "there are settings to show");

	UInt32 incomplete = 0;
	for (UInt32 at = 0; at < count; ++at) {
		const SettingDefinition& one = settings[at];
		const bool filled = one.Read != nullptr && one.Write != nullptr &&
		                    one.label != nullptr && one.label[0] != '\0' &&
		                    one.help != nullptr && one.help[0] != '\0' &&
		                    one.category != nullptr && one.category[0] != '\0';
		if (!filled) {
			std::printf("        row %u is incomplete\n", at);
			++incomplete;
		}
	}
	Check(incomplete == 0, "every row has a reader, a writer, a label and help");

	// A range that is empty or backwards makes a row that cannot be moved, and
	// it looks exactly like a row that is working until it is tried.
	UInt32 badRange = 0;
	for (UInt32 at = 0; at < count; ++at) {
		const SettingDefinition& one = settings[at];
		if (one.minimum > one.maximum) {
			std::printf("        row %u has a backwards range\n", at);
			++badRange;
		}
		if (one.kind == ItemKind::Number && one.step <= 0.0f) {
			std::printf("        row %u has no step\n", at);
			++badRange;
		}
	}
	Check(badRange == 0, "every row has a usable range and step");

	// Two rows with the same label are indistinguishable in the menu, and the
	// usual cause is a copied row whose label was never changed - the same
	// mistake the isolation test is looking for, showing up early.
	const char* duplicate = nullptr;
	for (UInt32 a = 0; a < count && duplicate == nullptr; ++a) {
		for (UInt32 b = a + 1; b < count; ++b) {
			if (TextIs(settings[a].label, settings[b].label)) {
				duplicate = settings[a].label;
				break;
			}
		}
	}
	if (duplicate != nullptr) {
		std::printf("        \"%s\" appears twice\n", duplicate);
	}
	Check(duplicate == nullptr, "no two rows share a label");
}

void TestIniKeys() {
	std::printf("Where each row lives in the INI\n");

	const SettingDefinition* const settings = SettingDefinitions();
	const UInt32 count = SettingDefinitionCount();

	// A row with no INI location cannot be saved, and a change to it is
	// overwritten by the next reload of the file - which is exactly how this
	// menu failed on its first run, and from inside a headset it looks like the
	// keys are being ignored rather than like a missing string.
	UInt32 missing = 0;
	for (UInt32 at = 0; at < count; ++at) {
		if (settings[at].iniSection == nullptr || settings[at].iniSection[0] == '\0' ||
		    settings[at].iniKey == nullptr || settings[at].iniKey[0] == '\0') {
			std::printf("        \"%s\" has nowhere to be saved\n", settings[at].label);
			++missing;
		}
	}
	Check(missing == 0, "every row says where in the INI it lives");

	// Two rows writing the same key means one of them overwrites the other on
	// every change - the same copied-row mistake as before, one layer further
	// out, and this time the damage is to the file rather than to memory.
	UInt32 collisions = 0;
	for (UInt32 a = 0; a < count; ++a) {
		for (UInt32 b = a + 1; b < count; ++b) {
			if (TextIs(settings[a].iniSection, settings[b].iniSection) &&
			    TextIs(settings[a].iniKey, settings[b].iniKey)) {
				std::printf("        \"%s\" and \"%s\" both write %s.%s\n", settings[a].label,
				            settings[b].label, settings[a].iniSection, settings[a].iniKey);
				++collisions;
			}
		}
	}
	Check(collisions == 0, "no two rows write the same INI key");

	// A worded setting needs both words or neither. Half a pair is a row
	// somebody was in the middle of editing, and it would write the word for
	// one state and a digit for the other.
	UInt32 halfWorded = 0;
	for (UInt32 at = 0; at < count; ++at) {
		const bool hasFalse = settings[at].falseWord != nullptr && settings[at].falseWord[0] != '\0';
		const bool hasTrue = settings[at].trueWord != nullptr && settings[at].trueWord[0] != '\0';
		if (hasFalse != hasTrue) {
			std::printf("        \"%s\" has only one of its two words\n", settings[at].label);
			++halfWorded;
		}

		// And only a switch can be worded. A number written as a word is a
		// value its own reader cannot parse.
		if (hasFalse && settings[at].kind != ItemKind::Toggle) {
			std::printf("        \"%s\" is a number but has words\n", settings[at].label);
			++halfWorded;
		}
	}
	Check(halfWorded == 0, "a worded setting has both its words, and is a switch");
}

void TestValuesAsWrittenToIni() {
	std::printf("What gets written into the file\n");

	using obvr::ui::FormatValueForIni;
	using obvr::ui::MenuItem;

	MenuItem toggle;
	toggle.kind = ItemKind::Toggle;

	char text[32];

	// A switch shows as "on" and has to be written as 1: Config reads it with a
	// numeric reader, and "on" would parse as nothing at all.
	toggle.value = 1.0f;
	FormatValueForIni(toggle, "", "", text, sizeof(text));
	Check(TextIs(text, "1"), "a switch that is on is written as 1");

	toggle.value = 0.0f;
	FormatValueForIni(toggle, "", "", text, sizeof(text));
	Check(TextIs(text, "0"), "and off, as 0");

	// The worded case. Menus is "cinema" or "world"; a 1 there is a value its
	// own reader rejects, logs, and then ignores - so the row would appear to
	// do nothing.
	toggle.value = 1.0f;
	FormatValueForIni(toggle, "cinema", "world", text, sizeof(text));
	Check(TextIs(text, "world"), "a worded switch writes its word");

	toggle.value = 0.0f;
	FormatValueForIni(toggle, "cinema", "world", text, sizeof(text));
	Check(TextIs(text, "cinema"), "and the other word for the other state");

	// Half a pair falls back to the number rather than writing an empty value,
	// which would leave the key present and blank.
	toggle.value = 1.0f;
	FormatValueForIni(toggle, "cinema", "", text, sizeof(text));
	Check(TextIs(text, "1"), "half a word pair falls back to the number");

	// A number is written as it is shown - the INI's readers take exactly that.
	MenuItem number;
	number.kind = ItemKind::Number;
	number.value = 1.25f;
	number.decimals = 2;
	FormatValueForIni(number, "", "", text, sizeof(text));
	Check(TextIs(text, "1.25"), "a number is written as it reads");

	number.value = 4224.0f;
	number.decimals = 0;
	FormatValueForIni(number, "", "", text, sizeof(text));
	Check(TextIs(text, "4224"), "and a whole one without a point");

	// Words are ignored for a number, since a number cannot be either of them.
	number.value = 2.0f;
	FormatValueForIni(number, "cinema", "world", text, sizeof(text));
	Check(TextIs(text, "2"), "a number ignores words");

	FormatValueForIni(number, "", "", nullptr, 0);
	Check(true, "and no buffer at all is survivable");
}

void TestCategoriesAreGrouped() {
	std::printf("Categories are in one piece\n");

	const SettingDefinition* const settings = SettingDefinitions();
	const UInt32 count = SettingDefinitionCount();

	// The menu draws a heading when the category changes, so a category that
	// appears, stops, and comes back draws its heading twice and looks like two
	// different sections with the same name.
	const char* scattered = nullptr;
	for (UInt32 at = 1; at < count && scattered == nullptr; ++at) {
		if (TextIs(settings[at].category, settings[at - 1].category)) {
			continue;
		}

		// A new category here: it must not have been seen before.
		for (UInt32 earlier = 0; earlier + 1 < at; ++earlier) {
			if (TextIs(settings[earlier].category, settings[at].category)) {
				scattered = settings[at].category;
				break;
			}
		}
	}
	if (scattered != nullptr) {
		std::printf("        \"%s\" is split up\n", scattered);
	}
	Check(scattered == nullptr, "each category appears as one run of rows");
}

void TestItemsAreBuilt() {
	std::printf("A row becomes a menu item\n");

	Config config;
	const SettingDefinition* const settings = SettingDefinitions();
	const UInt32 count = SettingDefinitionCount();

	UInt32 wrong = 0;
	for (UInt32 at = 0; at < count; ++at) {
		const SettingDefinition& one = settings[at];
		const obvr::ui::MenuItem item = ItemFor(one, config);

		if (!TextIs(item.label, one.label) || item.kind != one.kind ||
		    item.minimum != one.minimum || item.maximum != one.maximum ||
		    item.step != one.step || item.decimals != one.decimals ||
		    item.needsRestart != one.needsRestart || item.value != one.Read(config)) {
			std::printf("        row %u came out wrong\n", at);
			++wrong;
		}
	}
	Check(wrong == 0, "every row carries its own range, step and value across");

	// A half-finished row. It has to produce a visibly odd menu rather than
	// read through a null pointer.
	SettingDefinition empty;
	empty.minimum = 5.0f;
	const obvr::ui::MenuItem item = ItemFor(empty, config);
	Check(item.value == 5.0f, "a row with no reader sits at its minimum");

	ApplySetting(empty, config, 1.0f);
	Check(true, "and a row with no writer is survivable");
}

void TestValuesRoundTrip() {
	std::printf("A value written is a value read back\n");

	const SettingDefinition* const settings = SettingDefinitions();
	const UInt32 count = SettingDefinitionCount();

	UInt32 broken = 0;
	for (UInt32 at = 0; at < count; ++at) {
		Config config;
		const SettingDefinition& one = settings[at];

		const float wanted = DifferentValue(one, one.Read(config));
		ApplySetting(one, config, wanted);

		if (one.Read(config) != wanted) {
			std::printf("        row %u (\"%s\") did not keep what was written\n", at, one.label);
			++broken;
		}
	}
	Check(broken == 0, "every row reads back exactly what was written through it");

	// Clamping on the way in. A value from a saved menu state that no longer
	// matches the range must not be written through as it stands.
	UInt32 unclamped = 0;
	for (UInt32 at = 0; at < count; ++at) {
		Config config;
		const SettingDefinition& one = settings[at];

		ApplySetting(one, config, one.maximum + 1000.0f);
		if (one.Read(config) > one.maximum) {
			++unclamped;
		}

		ApplySetting(one, config, one.minimum - 1000.0f);
		if (one.Read(config) < one.minimum) {
			++unclamped;
		}
	}
	Check(unclamped == 0, "every row clamps a value to its own range");
}

void TestRowsDoNotDisturbEachOther() {
	std::printf("No row touches another row's setting\n");

	const SettingDefinition* const settings = SettingDefinitions();
	const UInt32 count = SettingDefinitionCount();

	// The check this file exists for. Write through one row, then read every
	// other row and confirm none of them moved. A row that writes the field it
	// was copied from shows up here and nowhere else.
	UInt32 collisions = 0;
	for (UInt32 changed = 0; changed < count; ++changed) {
		Config before;
		Config after;

		const SettingDefinition& one = settings[changed];
		const float wanted = DifferentValue(one, one.Read(after));
		ApplySetting(one, after, wanted);

		for (UInt32 other = 0; other < count; ++other) {
			if (other == changed) {
				continue;
			}
			if (settings[other].Read(after) != settings[other].Read(before)) {
				std::printf("        changing \"%s\" also changed \"%s\"\n", one.label,
				            settings[other].label);
				++collisions;
			}
		}
	}
	Check(collisions == 0, "changing any one setting leaves all the others alone");

	// And the other half of the same mistake: a row that writes correctly but
	// reads someone else's field would pass the round trip above only if both
	// happened to be the same field. Confirmed by writing through every row in
	// turn and checking each one ends up holding its own distinct value.
	Config config;
	for (UInt32 at = 0; at < count; ++at) {
		ApplySetting(settings[at], config, DifferentValue(settings[at], settings[at].Read(config)));
	}

	UInt32 lost = 0;
	for (UInt32 at = 0; at < count; ++at) {
		Config fresh;
		const float wanted = DifferentValue(settings[at], settings[at].Read(fresh));
		if (settings[at].Read(config) != wanted) {
			std::printf("        \"%s\" lost its value when the others were set\n",
			            settings[at].label);
			++lost;
		}
	}
	Check(lost == 0, "and every setting still holds its own value once all are set");
}

}  // namespace

int main() {
	std::printf("OBVR settings list test\n\n");

	TestTableIsComplete();
	std::printf("\n");
	TestIniKeys();
	std::printf("\n");
	TestValuesAsWrittenToIni();
	std::printf("\n");
	TestCategoriesAreGrouped();
	std::printf("\n");
	TestItemsAreBuilt();
	std::printf("\n");
	TestValuesRoundTrip();
	std::printf("\n");
	TestRowsDoNotDisturbEachOther();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
