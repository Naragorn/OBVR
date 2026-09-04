#pragma once

namespace obvr::game {

// Whether a scene graph node's name is one of a comma-separated list, the
// way [Hands] HideFirstPersonNodes spells it: "UpperBody, Hand". Case does
// not matter, spaces around the commas do not either, and an empty entry
// never matches - so a list of ",," hides nothing rather than everything.
//
// Pure, so the matching - the only decision in hiding a node - is checked
// without a scene graph.

inline char LowerAscii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; }

inline bool NameInList(const char* name, const char* list) {
	if (name == nullptr || list == nullptr || name[0] == '\0') {
		return false;
	}
	const char* at = list;
	while (*at != '\0') {
		// Skip leading spaces and commas to the entry's first character.
		while (*at == ' ' || *at == ',' || *at == '\t') {
			++at;
		}
		if (*at == '\0') {
			break;
		}
		const char* entry = at;
		while (*at != '\0' && *at != ',') {
			++at;
		}
		// Trim trailing spaces off the entry.
		const char* end = at;
		while (end > entry && (end[-1] == ' ' || end[-1] == '\t')) {
			--end;
		}
		// Compare the entry against the name, character for character.
		const char* n = name;
		const char* e = entry;
		while (e < end && *n != '\0' && LowerAscii(*e) == LowerAscii(*n)) {
			++e;
			++n;
		}
		if (e == end && *n == '\0') {
			return true;
		}
	}
	return false;
}

// Whether the list holds anything at all - so a blank setting is "hide
// nothing" without a walk of the tree.
inline bool ListIsEmpty(const char* list) {
	if (list == nullptr) {
		return true;
	}
	for (const char* at = list; *at != '\0'; ++at) {
		if (*at != ' ' && *at != ',' && *at != '\t') {
			return false;
		}
	}
	return true;
}

}  // namespace obvr::game
