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

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
