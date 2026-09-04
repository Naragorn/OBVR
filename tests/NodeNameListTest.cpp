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
