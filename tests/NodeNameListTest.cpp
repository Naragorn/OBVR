// Checks the node-name list matching behind [Hands] HideFirstPersonNodes:
// every way a list can be spelled, and every way a name can fail to be in
// it.

#include <cstdio>

#include "game/NodeNameList.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

}  // namespace

int main() {
	using obvr::game::ListIsEmpty;
	using obvr::game::NameInList;

	std::printf("Name in list\n");
	Check(NameInList("UpperBody", "UpperBody"), "a one-entry list matches its entry");
	Check(NameInList("upperbody", "UpperBody"), "case does not matter");
	Check(NameInList("Hand", "UpperBody, Hand"), "the second entry, after a comma and a space");
	Check(NameInList("Hand", "UpperBody,Hand"), "or without the space");
	Check(NameInList("Hand", " Hand , UpperBody "), "spaces around an entry are trimmed");
	Check(!NameInList("Upper", "UpperBody"), "a prefix of an entry is not the entry");
	Check(!NameInList("UpperBodyX", "UpperBody"), "nor is an entry with more after it");
	Check(!NameInList("Body", "UpperBody"), "nor a suffix");
	Check(!NameInList("Hand", "UpperBody"), "a name not listed is not matched");
	Check(!NameInList("", "UpperBody"), "an empty name matches nothing");
	Check(!NameInList("Hand", ""), "an empty list matches nothing");
	Check(!NameInList("Hand", ",,"), "a list of commas matches nothing");
	Check(!NameInList("Hand", ", ,"), "nor a list of commas and spaces");
	Check(!NameInList(nullptr, "Hand"), "no name, no match");
	Check(!NameInList("Hand", nullptr), "no list, no match");
	Check(NameInList("Bip01 R Hand", "UpperBody, Bip01 R Hand"),
	      "an entry with spaces inside is matched whole");
	Check(NameInList("a", "b,a,c"), "a single-character entry in the middle");
	Check(NameInList("Foot:1", "LowerBody,Foot"), "a numbered part matches by its name before the colon");
	Check(NameInList("upperbody:0", "UpperBody"), "case does not matter there either");
	Check(!NameInList("Foot:1", "Foot:2"), "a listed number is matched whole");
	Check(NameInList("Foot:2", "Foot:2"), "and matches itself");
	Check(!NameInList("Footwear:1", "Foot"), "the part before the colon must be the whole entry");
	Check(!NameInList(":1", "Foot"), "a name that is only a number matches nothing");

	using obvr::game::HiddenUnderCuirass;
	using obvr::game::IsWornItemNode;

	std::printf("Worn item node\n");
	Check(IsWornItemNode("UpperBody  (0001C6D1)", "UpperBody"), "the slot, two spaces and the form ID: the worn item's node");
	Check(IsWornItemNode("upperbody (0001C6D1)", "UpperBody"), "case and one space do not matter");
	Check(IsWornItemNode("Hand  (000661C2)", "Hand"), "another slot by its own name");
	Check(!IsWornItemNode("Hand  (000661C2)", "UpperBody"), "a different slot is not this one");
	Check(!IsWornItemNode("UpperBody", "UpperBody"), "a shape named UpperBody is not a worn item's node");
	Check(!IsWornItemNode("UpperBody:0", "UpperBody"), "nor a numbered one");
	Check(!IsWornItemNode("UpperBodyX  (0001C6D1)", "UpperBody"), "the slot's name must end where the spaces start");
	Check(!IsWornItemNode("UpperBody  0001C6D1", "UpperBody"), "no bracket after the spaces: not a form ID");
	Check(!IsWornItemNode("Upper  (0001C6D1)", "UpperBody"), "a part of the slot's name is not the slot");
	Check(!IsWornItemNode(nullptr, "UpperBody") && !IsWornItemNode("UpperBody  (1)", nullptr),
	      "no name or no slot: no");
	Check(!IsWornItemNode("UpperBody  (1)", ""), "an empty slot matches nothing");

	std::printf("Hidden under the cuirass\n");
	Check(HiddenUnderCuirass(true, "cuirass", "Hand,Arms"), "a torso shape under the cuirass goes");
	Check(HiddenUnderCuirass(true, "Foot:Body", "Hand,Arms"), "the body shape a cuirass carries goes");
	Check(HiddenUnderCuirass(true, "Pauldrons", "Hand,Arms"), "the pauldrons go");
	Check(!HiddenUnderCuirass(true, "Hand", "Hand,Arms"), "a hand in the cuirass stays");
	Check(!HiddenUnderCuirass(true, "Arms:0", "Hand,Arms"), "the arms are left to the list and the stump");
	Check(!HiddenUnderCuirass(false, "cuirass", "Hand,Arms"), "outside the cuirass nothing goes by this rule");
	Check(!HiddenUnderCuirass(true, "cuirass", nullptr), "no keep list: the rule is off");
	Check(HiddenUnderCuirass(true, "Hand", ""), "an empty keep list keeps nothing");
	Check(!HiddenUnderCuirass(true, "", "Hand,Arms"), "a shape without a name is left alone");

	std::printf("List is empty\n");
	Check(ListIsEmpty(""), "an empty string");
	Check(ListIsEmpty(" , ,"), "commas and spaces");
	Check(ListIsEmpty(nullptr), "no list");
	Check(!ListIsEmpty("UpperBody"), "a name is not empty");
	Check(!ListIsEmpty(" ,x"), "nor a name after commas");

	using obvr::game::ShapeHiddenByList;
	using obvr::game::WornSlotLength;

	std::printf("The slot of a worn item's node\n");
	Check(WornSlotLength("Hand  (000BE335)") == 4, "\"Hand  (000BE335)\": the slot Hand");
	Check(WornSlotLength("UpperBody (0001C6D1)") == 9, "one space before the bracket: UpperBody");
	Check(WornSlotLength("Hand") == 0, "a shape named Hand is no worn item");
	Check(WornSlotLength("Foot:handRM") == 0, "nor a part named with a colon");
	Check(WornSlotLength("Bip01 R Hand") == 0, "nor a bone with spaces in its name");
	Check(WornSlotLength("  (000BE335)") == 0 && WornSlotLength(nullptr) == 0, "no slot word, no slot");

	// The male hand of the body the testers use names its shape "Foot:handRM"
	// (characters\_male\hand.nif, read 2026-11-10 on both installs): taken by
	// "Foot" - the boots' entry - for a foot, it was hidden in Full VR, and a
	// male character had no first-person hands from the race menu's Done on
	// (Nadi, 2026-11-10). Under a worn item's node the item's slot decides,
	// not a part's name: an entry naming another slot does not reach it.
	std::printf("A part under a worn item's node\n");
	const char* const arms = "LowerBody,Foot";
	Check(!ShapeHiddenByList("Foot:handRM", "Hand  (00000907)", arms),
	      "the male hand \"Foot:handRM\" under the hand's node: not hidden by the boots' Foot");
	Check(ShapeHiddenByList("Foot:handRM", "Hand  (00000907)", "LowerBody,Foot,Hand,Weapon"),
	      "the hands away (Hand listed): hidden by its slot, whatever its name");
	Check(ShapeHiddenByList("Foot:0", "Foot  (00023B56)", arms), "a boot's part under the foot's node: hidden");
	Check(ShapeHiddenByList("Shoe", "Foot  (00023B56)", arms), "any part of the boots: hidden by the slot");
	Check(!ShapeHiddenByList("Foot:handRM", nullptr, "Hand"), "no worn node above: by its own name, not Hand");
	Check(ShapeHiddenByList("Foot:handRM", nullptr, arms), "no worn node above: by its own name, Foot as before");
	Check(ShapeHiddenByList("Hand", "Hand  (000661C2)", "Hand"), "the hand's own \"Hand\" with Hand listed: hidden");
	Check(!ShapeHiddenByList("Hand", "Hand  (000661C2)", arms), "and with only the boots listed: shown");
	Check(ShapeHiddenByList("ArmL", "UpperBody  (0001C6D1)", "ArmL,LowerBody,Foot"),
	      "an arm mesh named in the list under the cuirass: hidden by its own name");
	Check(!ShapeHiddenByList("Foot:Body", "UpperBody  (0001C6D1)", arms),
	      "a body part named Foot: under the cuirass: not the boots' (the cuirass rule decides there)");
	Check(ShapeHiddenByList("Weapon", "Hand  (00000907)", "Weapon,Torch"),
	      "the weapon's node by its name under the hand: hidden (Weapon names a node, not a body slot)");
	Check(ShapeHiddenByList("Bip01 L ForearmTwist", "Hand  (00000907)", "Bip01 L ForearmTwist"),
	      "a bone by its name under the hand: hidden");
	// Kept in view by the same rule (KeepFirstPersonNodesInView with "Hand"):
	// the male hand under the hand's node counts as a hand.
	Check(ShapeHiddenByList("Foot:handRM", "Hand  (000BE335)", "Hand"),
	      "the list \"Hand\" takes the male hand under the hand's node (kept in view with it)");
	Check(!ShapeHiddenByList(nullptr, "Hand  (00000907)", arms) && !ShapeHiddenByList("Hand", "Hand  (1)", nullptr),
	      "no name or no list: nothing hidden");

	using obvr::game::ListEntry;
	using obvr::game::MeasureEntry;
	using obvr::game::NameIsEntry;

	std::printf("A walk's name measured once\n");
	{
		const ListEntry hand = MeasureEntry("Bip01 R Hand");
		Check(hand.one && hand.length == 12, "one entry: measured, its length");
		const ListEntry trimmed = MeasureEntry(" \tHand \t");
		Check(trimmed.one && trimmed.length == 4 && trimmed.entry[0] == 'H', "spaces and tabs round it are not the entry");
		Check(MeasureEntry(", Hand").one, "separators before the one entry are skipped");
		Check(!MeasureEntry("UpperBody, Hand").one && !MeasureEntry("Hand,").one,
		      "a comma after the entry: a list, left to NameInList");
		Check(!MeasureEntry(nullptr).one, "no list: not measured");
		const ListEntry blank = MeasureEntry("  ");
		Check(blank.one && blank.length == 0 && !NameIsEntry("Hand", 4, blank), "a blank list: an entry of nothing, never a match");
		Check(NameIsEntry("bip01 r hand", 12, hand), "the name itself, case aside");
		Check(NameIsEntry("Bip01 R Hand:0", 14, hand), "the name with a part number");
		Check(!NameIsEntry("Bip01 L Hand", 12, hand), "another name of the same length");
		Check(!NameIsEntry("Bip01 R Hand2", 13, hand), "a longer name without the colon there");
		Check(!NameIsEntry("Bip01 R Han", 11, hand), "a shorter name");
		Check(!NameIsEntry("", 0, hand) && !NameIsEntry(nullptr, 0, hand), "no name: no match");
		Check(NameIsEntry("Hand", 4, MeasureEntry("UpperBody, Hand")), "a list answers as NameInList does");
	}
	// The same answer as NameInList for every name against every list.
	{
		static const char* const kNames[] = {"",          "a",         "Hand",       "hand",      "Hand:0",   "Hand:",
		                                     ":1",        "Foot:1",    "Foot:2",     "Footwear:1", "Foot",     "Foo",
		                                     "UpperBody", "upperbody:0", "Upper",    "UpperBodyX", "Body",     "Arrow:0",
		                                     "Arrow:0:1", "Arrow:1",   "Bip01 R Hand", "Bip01 L Hand", "Bip01", "Bip01 "};
		static const char* const kLists[] = {"",      " ",      ",,",        ", ,",     "a",       "b,a,c",   "Hand",
		                                     " Hand ", "\tHand", "Hand,",     ",Hand",   "hand",    "Foot",    "Foot:2",
		                                     "Arrow:0", "Arrow", "UpperBody", "UpperBody, Hand", "UpperBody,Hand",
		                                     " Hand , UpperBody ", "Bip01 R Hand", "Bip01", "LowerBody,Foot", "Hand:"};
		unsigned differing = 0;
		unsigned matches = 0;
		for (const char* name : kNames) {
			unsigned length = 0;
			while (name[length] != '\0') {
				++length;
			}
			for (const char* list : kLists) {
				const bool expected = NameInList(name, list);
				matches += expected ? 1 : 0;
				differing += NameIsEntry(name, length, MeasureEntry(list)) != expected ? 1 : 0;
			}
		}
		Check(differing == 0, "every name against every list: the measured entry answers as NameInList");
		Check(matches > 30, "and the sweep holds matches as well as misses");
	}

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
