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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

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
using obvr::ui::FindSetting;
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
		// A button has nothing to save and therefore nowhere to save it; it
		// must say so by having no INI location at all.
		if (settings[at].kind == ItemKind::Action) {
			if (settings[at].iniKey[0] != '\0' || settings[at].iniSection[0] != '\0') {
				std::printf("        \"%s\" is a button with an INI home\n", settings[at].label);
				++missing;
			}
			continue;
		}
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
		if (settings[a].kind == ItemKind::Action) {
			continue;
		}
		for (UInt32 b = a + 1; b < count; ++b) {
			if (settings[b].kind == ItemKind::Action) {
				continue;
			}
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
	UInt32 unexpectedWorded = 0;
	UInt32 expectedWorded = 0;
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

		// Only the three settings whose Config readers deliberately name places
		// are words. Ordinary boolean readers accept digits, so descriptive
		// words on their rows would be written successfully and then ignored on
		// the next reload (the crosshair toggles once did exactly that).
		const bool mayBeWorded =
			(TextIs(settings[at].iniSection, "Render") &&
			 TextIs(settings[at].iniKey, "HudAnchor")) ||
			(TextIs(settings[at].iniSection, "Render") &&
			 TextIs(settings[at].iniKey, "Menus")) ||
			(TextIs(settings[at].iniSection, "SettingsMenu") &&
			 TextIs(settings[at].iniKey, "Anchor"));
		if (hasFalse && mayBeWorded) {
			++expectedWorded;
		} else if (hasFalse) {
			std::printf("        \"%s\" writes words but its Config reader expects a number\n",
			            settings[at].label);
			++unexpectedWorded;
		}
	}
	Check(halfWorded == 0, "a worded setting has both its words, and is a switch");
	Check(unexpectedWorded == 0, "only settings with word-aware readers write words");
	Check(expectedWorded == 3, "all three word-aware settings keep their readable words");

	const SettingDefinition* easing = FindSetting("Render", "CrosshairDepthSpeed");
	Check(easing != nullptr && easing->minimum == 0.0f,
	      "crosshair easing exposes 0 for instant depth changes");
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

void TestMenuSections() {
	std::printf("The menu's sections, by topic (2026-11-07)\n");

	// The sections in the order the menu lists them: the most used first.
	const char* const order[] = {"Start & help",       "Moving",           "Combat: features",
	                             "People & enemies",   "Combat: tuning",   "Weapon physics",
	                             "Bow",                "Grab & throw",     "Holsters & items",
	                             "Reaching & opening", "Laser & wrist menus", "HUD",
	                             "Body & hands",       "Comfort",          "Looking",
	                             "Aiming",             "Dialogue",         "Screen & menus",
	                             "Picture & performance"};
	const UInt32 wanted = static_cast<UInt32>(sizeof(order) / sizeof(order[0]));
	const SettingDefinition* const settings = SettingDefinitions();
	const UInt32 count = SettingDefinitionCount();
	UInt32 seen = 0;
	bool inOrder = true;
	for (UInt32 at = 0; at < count; ++at) {
		if (at > 0 && TextIs(settings[at - 1].category, settings[at].category)) {
			continue;
		}
		if (seen >= wanted || !TextIs(settings[at].category, order[seen])) {
			std::printf("        section %u is \"%s\"\n", seen, settings[at].category);
			inOrder = false;
		}
		++seen;
	}
	Check(inOrder && seen == wanted, "the sections in their order, each once");

	// The feature switches stand together, switches only.
	const char* const features[] = {"Kick", "ShieldBash", "PommelStrikes", "HalfSwording", "Dismemberment",
	                                "MotionHits", "BlockByAngle", "TorchBlocks", "TorchDrop", "VanillaKnockdown"};
	bool together = true;
	for (const char* key : features) {
		const SettingDefinition* row = FindSetting("Hands", key);
		together = together && row != nullptr && TextIs(row->category, "Combat: features");
	}
	Check(together, "kick, shield bash, pommel, half-swording, heads and the rest under Combat: features");
	bool onlySwitches = true;
	for (UInt32 at = 0; at < count; ++at) {
		if (TextIs(settings[at].category, "Combat: features")) {
			onlySwitches = onlySwitches && settings[at].kind == ItemKind::Toggle;
		}
	}
	Check(onlySwitches, "Combat: features holds switches only - its numbers are under Combat: tuning");
	const SettingDefinition* yield = FindSetting("Hands", "Yield");
	const SettingDefinition* ring = FindSetting("Hands", "CombatRing");
	Check(yield != nullptr && ring != nullptr && TextIs(yield->category, "People & enemies") &&
	          TextIs(ring->category, "People & enemies"),
	      "the yield and the fighters' ring under People & enemies");

	// Rows a tester looked for by name: the grab's reach, the hands' pitch
	// beside their roll and yaw (2026-10-17: "no pitch row for the hands"),
	// the stick's turn beside the snap turn ("smooth turning is waaaay too
	// slow" - the row was nowhere), the controllers' rows with the mode.
	// The view at the player's own height: seated players switch it off in
	// the menu, not only in the INI.
	const SettingDefinition* realHeight = FindSetting("Head", "RealHeight");
	Check(realHeight != nullptr && TextIs(realHeight->category, "Comfort") && realHeight->kind == ItemKind::Toggle,
	      "the view at your own height has a switch under Comfort");
	if (realHeight != nullptr) {
		Config c;
		ApplySetting(*realHeight, c, 0.0f);
		const bool off = !c.tracker.realHeight && realHeight->Read(c) == 0.0f;
		ApplySetting(*realHeight, c, 1.0f);
		Check(off && c.tracker.realHeight && realHeight->Read(c) == 1.0f, "its switch turns RealHeight off and on");
	}
	// The buzz when a blade, a hand, a shield or a foot meets something: a
	// switch, off unless asked for (Nadi, 2026-11-08).
	const SettingDefinition* contact = FindSetting("Hands", "ContactHaptics");
	Check(contact != nullptr && contact->kind == ItemKind::Toggle && TextIs(contact->category, "Combat: features"),
	      "the contact buzz has a switch under Combat: features");
	if (contact != nullptr) {
		Config c;
		const bool offByDefault = !c.hands.contactHaptics && contact->Read(c) == 0.0f;
		ApplySetting(*contact, c, 1.0f);
		Check(offByDefault && c.hands.contactHaptics, "the contact buzz is off by default and its switch turns it on");
	}
	// Split from it (Nadi, 2026-11-08): the buzz of a blow landing, on unless
	// switched off.
	const SettingDefinition* hit = FindSetting("Hands", "HitHaptics");
	Check(hit != nullptr && hit->kind == ItemKind::Toggle && TextIs(hit->category, "Combat: features"),
	      "the buzz of a blow landing has its own switch under Combat: features");
	if (hit != nullptr) {
		Config c;
		const bool onByDefault = c.hands.hitHaptics && hit->Read(c) == 1.0f;
		ApplySetting(*hit, c, 0.0f);
		Check(onByDefault && !c.hands.hitHaptics && !c.hands.contactHaptics,
		      "the hit buzz is on by default, its switch turns it off, and it leaves the touch buzz alone");
	}
	// The stab that sticks (docs/blade-penetration-spec.md 10): a switch, on
	// unless switched off.
	const SettingDefinition* sticks = FindSetting("Hands", "BladeSticks");
	Check(sticks != nullptr && sticks->kind == ItemKind::Toggle && TextIs(sticks->category, "Combat: features"),
	      "the stab that sticks has a switch under Combat: features");
	if (sticks != nullptr) {
		Config c;
		const bool onByDefault = c.hands.bladeEntry.stick.enabled && sticks->Read(c) == 1.0f;
		ApplySetting(*sticks, c, 0.0f);
		const bool off = !c.hands.bladeEntry.stick.enabled && sticks->Read(c) == 0.0f;
		ApplySetting(*sticks, c, 1.0f);
		Check(onByDefault && off && c.hands.bladeEntry.stick.enabled && c.hands.bladeEntry.enabled,
		      "stabs stick by default, its switch turns that off and on, and leaves the entry itself alone");
	}
	const SettingDefinition* grab = FindSetting("Hands", "GrabReachMetres");
	Check(grab != nullptr && TextIs(grab->category, "Grab & throw"), "the grab's reach under Grab & throw");
	const SettingDefinition* rightPitch = FindSetting("Hands", "RightHandPitch");
	const SettingDefinition* leftPitch = FindSetting("Hands", "LeftHandPitch");
	Check(rightPitch != nullptr && leftPitch != nullptr && TextIs(rightPitch->category, "Body & hands") &&
	          TextIs(leftPitch->category, "Body & hands") && TextIs(rightPitch->label, "Right hand pitch") &&
	          TextIs(leftPitch->label, "Left hand pitch") && rightPitch->kind == ItemKind::Number &&
	          rightPitch->minimum <= -180.0f && rightPitch->maximum >= 180.0f,
	      "each hand's pitch has its row under Body & hands, a number over the whole turn");
	const SettingDefinition* stickTurn = FindSetting("Hands", "TurnSpeed");
	const SettingDefinition* easedTurn = FindSetting("Look", "TurnSpeed");
	const SettingDefinition* snap = FindSetting("Look", "SnapTurning");
	Check(stickTurn != nullptr && snap != nullptr && TextIs(stickTurn->category, "Moving") &&
	          TextIs(snap->category, "Moving") && TextIs(stickTurn->label, "Stick turn speed") &&
	          stickTurn->kind == ItemKind::Number && stickTurn->minimum <= 1.0f && stickTurn->maximum >= 48.0f,
	      "the stick's turn speed beside the snap turn under Moving, up to four times the default");
	Check(easedTurn != nullptr && !TextIs(easedTurn->label, stickTurn == nullptr ? "" : stickTurn->label),
	      "the eased turn's row keeps its own name");
	const SettingDefinition* menus = FindSetting("Hands", "ControllerMenus");
	const SettingDefinition* mode = FindSetting("Hands", "Enabled");
	Check(menus != nullptr && mode != nullptr && TextIs(menus->category, "Start & help") &&
	          TextIs(mode->category, "Start & help"),
	      "the mode switch and the controllers' menu row under Start & help");
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

void TestCanonicalResetRules() {
	std::printf("Canonical defaults and reset eligibility\n");

	const SettingDefinition* const settings = SettingDefinitions();
	const UInt32 count = SettingDefinitionCount();
	const Config defaults;

	UInt32 missingDefaults = 0;
	UInt32 outsideBounds = 0;
	UInt32 defaultRowsResettable = 0;
	UInt32 changedRowsNotResettable = 0;
	for (UInt32 at = 0; at < count; ++at) {
		const SettingDefinition& one = settings[at];
		float canonical = 0.0f;
		if (!obvr::ui::CanonicalDefaultValue(one, canonical)) {
			++missingDefaults;
			continue;
		}
		if (canonical < one.minimum || canonical > one.maximum) {
			std::printf("        \"%s\" default is outside its range\n", one.label);
			++outsideBounds;
		}

		// Actions have a reader only so the table can be swept uniformly, but
		// they are not settings and must never become reset proposals.
		if (one.kind == ItemKind::Action || one.kind == ItemKind::Text) {
			if (obvr::ui::CanResetSetting(one, defaults)) {
				++defaultRowsResettable;
			}
			continue;
		}

		if (obvr::ui::CanResetSetting(one, defaults)) {
			++defaultRowsResettable;
		}

		Config changed = defaults;
		ApplySetting(one, changed, DifferentValue(one, canonical));
		if (!obvr::ui::CanResetSetting(one, changed)) {
			std::printf("        \"%s\" changed value is not resettable\n", one.label);
			++changedRowsNotResettable;
		}
	}
	Check(missingDefaults == 0, "every definition can read its canonical default");
	Check(outsideBounds == 0, "every canonical default lies within its menu bounds");
	Check(defaultRowsResettable == 0, "actions and default-valued rows refuse reset");
	Check(changedRowsNotResettable == 0, "every changed editable row becomes resettable");

	SettingDefinition missingReader;
	missingReader.kind = ItemKind::Number;
	missingReader.minimum = 0.0f;
	missingReader.maximum = 1.0f;
	missingReader.Write = +[](Config&, float) {};
	float ignored = 0.0f;
	Check(!obvr::ui::CanonicalDefaultValue(missingReader, ignored),
	      "a malformed row without a reader has no canonical default");
	Check(!obvr::ui::CanResetSetting(missingReader, defaults),
	      "a malformed row without a reader refuses reset");

	SettingDefinition missingWriter;
	missingWriter.kind = ItemKind::Number;
	missingWriter.Read = +[](const Config&) { return 1.0f; };
	Check(!obvr::ui::CanResetSetting(missingWriter, defaults),
	      "a malformed row without a writer refuses reset");

	SettingDefinition textRow;
	textRow.kind = ItemKind::Text;
	textRow.Read = +[](const Config&) { return 0.0f; };
	textRow.Write = +[](Config&, float) {};
	Check(!obvr::ui::CanResetSetting(textRow, defaults),
	      "a text row refuses reset even with reader and writer callbacks");
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

void TestLaserNeedsBeamOrDot() {
	std::printf("The laser keeps the beam or the dot\n");
	using obvr::ui::SettingEditRefusal;
	const SettingDefinition* beam = FindSetting("Hands", "LaserBeam");
	const SettingDefinition* dot = FindSetting("Hands", "LaserDot");
	Check(beam != nullptr && dot != nullptr, "both are rows in the menu");
	if (beam == nullptr || dot == nullptr) {
		return;
	}
	Config config;
	config.hands.laserBeam = true;
	config.hands.laserDot = true;
	Check(SettingEditRefusal(*beam, config, 0.0f) == nullptr, "the beam off with the dot on: fine");
	Check(SettingEditRefusal(*dot, config, 0.0f) == nullptr, "the dot off with the beam on: fine");
	config.hands.laserDot = false;
	Check(SettingEditRefusal(*beam, config, 0.0f) != nullptr, "the beam off with the dot off: refused");
	Check(SettingEditRefusal(*beam, config, 1.0f) == nullptr, "the beam on: always fine");
	config.hands.laserDot = true;
	config.hands.laserBeam = false;
	Check(SettingEditRefusal(*dot, config, 0.0f) != nullptr, "the dot off with the beam off: refused");
	const SettingDefinition* other = FindSetting("Hands", "LeftHanded");
	Check(other != nullptr && SettingEditRefusal(*other, config, 0.0f) == nullptr,
	      "other rows are never refused by it");
	const SettingDefinition* radius = FindSetting("Look", "SnapTurnVignetteRadius");
	Check(radius != nullptr && radius->minimum == 5.0f && radius->maximum == 30.0f &&
	          radius->step == 1.0f,
	      "the vignette radius goes 5 to 30 in steps of one");
}

namespace {

// The whole table pinned, row by row and in order: what each row shows, where
// it writes, its range, its default and what it reads from a fresh Config.
// tests/settings-rows.txt holds the list; moving the table between files
// (2026-11-11, the size ratchet) must leave it byte for byte. A row added or
// changed on purpose rewrites it: settings_list_test --print > tests/settings-rows.txt
std::string RowLine(const SettingDefinition& d) {
	auto text = [](const char* t) { return std::string(t != nullptr ? t : "(null)"); };
	char numbers[160];
	float fallback = 0.0f;
	const bool hasDefault = obvr::ui::CanonicalDefaultValue(d, fallback);
	const Config fresh;
	const float read = d.Read != nullptr ? d.Read(fresh) : -12345.0f;
	std::snprintf(numbers, sizeof(numbers), "%d|%.6g|%.6g|%.6g|%u|%d|%d|%.6g|%.6g|%d|%u",
	              static_cast<int>(d.kind), static_cast<double>(d.minimum), static_cast<double>(d.maximum),
	              static_cast<double>(d.step), d.decimals, d.needsRestart ? 1 : 0, hasDefault ? 1 : 0,
	              static_cast<double>(fallback), static_cast<double>(read), static_cast<int>(d.action),
	              d.valueNameCount);
	std::string line = text(d.category) + "|" + text(d.label) + "|" + text(d.help) + "|" + text(d.iniSection) +
	                   "|" + text(d.iniKey) + "|" + text(d.falseWord) + "|" + text(d.trueWord) + "|" + numbers;
	for (UInt32 i = 0; i < d.valueNameCount && d.valueNames != nullptr; ++i) {
		line += "|" + text(d.valueNames[i]);
	}
	line += d.Read != nullptr ? "|R" : "|-";
	line += d.Write != nullptr ? "W" : "-";
	return line;
}

std::string TableText() {
	std::string out;
	for (UInt32 i = 0; i < SettingDefinitionCount(); ++i) {
		out += RowLine(SettingDefinitions()[i]) + "\n";
	}
	return out;
}

void TestTableMatchesPinnedRows() {
	std::printf("The table, row by row, as tests/settings-rows.txt pins it\n");
	std::ifstream in(std::string(OBVR_REPO_DIR) + "/tests/settings-rows.txt", std::ios::binary);
	std::stringstream pinned;
	pinned << in.rdbuf();
	Check(in.good() || in.eof(), "tests/settings-rows.txt read");
	// A Windows checkout may turn the file's line ends into CR LF (git's
	// autocrlf): the rows are compared without the CR.
	std::string pinnedText = pinned.str();
	pinnedText.erase(std::remove(pinnedText.begin(), pinnedText.end(), '\r'), pinnedText.end());
	const std::string now = TableText();
	if (now != pinnedText) {
		// the first differing line, to find it without a diff tool
		std::istringstream a(now), b(pinnedText);
		std::string la, lb;
		for (int n = 1; n < 100000; ++n) {
			const bool moreA = static_cast<bool>(std::getline(a, la));
			const bool moreB = static_cast<bool>(std::getline(b, lb));
			if (!moreA && !moreB) {
				break;
			}
			if (la != lb) {
				std::printf("  line %d now:    %s\n  line %d pinned: %s\n", n, la.c_str(), n, lb.c_str());
				break;
			}
			la.clear();
			lb.clear();
		}
	}
	Check(now == pinnedText, "every row as pinned: label, help, INI key, range, default, order");
}

}  // namespace

int main(int argc, char** argv) {
	if (argc > 1 && std::strcmp(argv[1], "--print") == 0) {
		std::fputs(TableText().c_str(), stdout);
		return 0;
	}
	std::printf("OBVR settings list test\n\n");

	TestTableIsComplete();
	std::printf("\n");
	TestIniKeys();
	std::printf("\n");
	TestValuesAsWrittenToIni();
	std::printf("\n");
	TestCategoriesAreGrouped();
	TestMenuSections();
	std::printf("\n");
	TestItemsAreBuilt();
	std::printf("\n");
	TestCanonicalResetRules();
	std::printf("\n");
	TestValuesRoundTrip();
	std::printf("\n");
	TestRowsDoNotDisturbEachOther();
	std::printf("\n");
	TestLaserNeedsBeamOrDot();
	std::printf("\n");
	TestTableMatchesPinnedRows();

	std::printf("\n");
	if (g_failures == 0) {
		std::printf("All checks passed.\n");
		return 0;
	}

	std::printf("%d check(s) failed.\n", g_failures);
	return 1;
}
