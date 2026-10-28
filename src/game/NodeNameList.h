#pragma once

namespace obvr::game {

// Whether a scene graph node's name is one of a comma-separated list, the
// way [Hands] HideFirstPersonNodes spells it: "UpperBody, Hand". Case does
// not matter, spaces around the commas do not either, and an empty entry
// never matches - so a list of ",," hides nothing rather than everything.
// A name with a colon matches by what comes before it, the way the engine
// reads its own "UpperBody:X" as UpperBody (cs.uesp.net, "NifSkope/Modeling
// Tutorial: Fixing the armor incorrectly displayed in 1st person"): an
// armour made of several "Foot:0", "Foot:1" parts is hidden as "Foot".
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
		if (e == end && (*n == '\0' || *n == ':')) {
			return true;
		}
	}
	return false;
}

// Whether a node's name is the engine's node for an item worn in a slot: the
// slot's name, spaces, then the item's form ID in brackets - "UpperBody
// (0001C6D1)", "Hand  (000661C2)" (OBVR.log, 2026-10-28, the "Menu near
// shapes" lines). A shape named "UpperBody" inside a mesh is not one.
inline bool IsWornItemNode(const char* name, const char* slot) {
	if (name == nullptr || slot == nullptr || slot[0] == '\0') {
		return false;
	}
	const char* n = name;
	for (const char* s = slot; *s != '\0'; ++s, ++n) {
		if (*n == '\0' || LowerAscii(*n) != LowerAscii(*s)) {
			return false;
		}
	}
	if (*n != ' ') {
		return false;
	}
	while (*n == ' ') {
		++n;
	}
	return *n == '(';
}

// Whether a shape under the first-person model goes as part of the worn
// cuirass in Full VR: every shape under the cuirass's node except those named
// in keep - the hands, and the arm meshes the hide list and the forearm stump
// decide on. The engine hides only shapes named "UpperBody" in first person
// and draws every other (cs.uesp.net, "NifSkope/Modeling Tutorial: Fixing
// the armor incorrectly displayed in 1st person"); a cuirass whose torso
// carries other names - "cuirass", "straps", "Pauldrons", the body
// "Foot:Body" - hung before the eyes and turned with the head and the hand
// (61 female cuirasses without an "UpperBody" shape, Vanilla Gear Redux's
// unreasonable flavour and BreakArmor meshes, 2026-10-28). Flat, the
// first-person model sits behind the camera; in Full VR it does not. Null
// keep is the rule off; a shape without a name is left alone.
inline bool HiddenUnderCuirass(bool underCuirass, const char* shapeName, const char* keep) {
	if (!underCuirass || keep == nullptr || shapeName == nullptr || shapeName[0] == '\0') {
		return false;
	}
	return !NameInList(shapeName, keep);
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
