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

// A name looked for in a walk of a tree, measured once before the walk: where
// its entry begins and how long it is. NameIsEntry then answers NameInList
// for it from the node name's length in all but the names of that length -
// NameInList reads the entry to its end again for every node, and the body's
// bones are 27 names against every node of the tree, every frame
// (docs/performance-spec.md, "the model trees walked by name"). A list of
// more than one entry is not measured (`one` false) and is NameInList's.
struct ListEntry {
	const char* list = nullptr;   // as it was given
	const char* entry = nullptr;  // the one entry's first character
	unsigned length = 0;
	bool one = false;
};

inline ListEntry MeasureEntry(const char* list) {
	ListEntry measured;
	measured.list = list;
	if (list == nullptr) {
		return measured;
	}
	const char* at = list;
	while (*at == ' ' || *at == ',' || *at == '\t') {
		++at;
	}
	const char* const entry = at;
	while (*at != '\0' && *at != ',') {
		++at;
	}
	if (*at == ',') {
		return measured;
	}
	while (at > entry && (at[-1] == ' ' || at[-1] == '\t')) {
		--at;
	}
	measured.entry = entry;
	measured.length = static_cast<unsigned>(at - entry);
	measured.one = true;
	return measured;
}

// NameInList(name, the list `wanted` was measured from), with the name's
// length (its characters before the end) known.
inline bool NameIsEntry(const char* name, unsigned nameLength, const ListEntry& wanted) {
	if (!wanted.one) {
		return NameInList(name, wanted.list);
	}
	if (name == nullptr || wanted.length == 0 || nameLength < wanted.length) {
		return false;
	}
	if (nameLength > wanted.length && name[wanted.length] != ':') {
		return false;
	}
	for (unsigned i = 0; i < wanted.length; ++i) {
		if (LowerAscii(name[i]) != LowerAscii(wanted.entry[i])) {
			return false;
		}
	}
	return true;
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

// The slot word of a worn item's node - "Hand  (000BE335)" is the slot Hand,
// four characters - or 0 for any other name: letters, then one or more
// spaces, then the bracket of the item's form ID (IsWornItemNode's pattern
// for any slot).
inline unsigned WornSlotLength(const char* name) {
	if (name == nullptr) {
		return 0;
	}
	unsigned letters = 0;
	while ((name[letters] >= 'A' && name[letters] <= 'Z') || (name[letters] >= 'a' && name[letters] <= 'z')) {
		++letters;
	}
	if (letters == 0 || name[letters] != ' ') {
		return 0;
	}
	const char* n = name + letters;
	while (*n == ' ') {
		++n;
	}
	return *n == '(' ? letters : 0;
}

// Whether `entry` (`length` characters) names one of the body's slots an item
// is worn in - not the nodes named after the weapon, the torch or the
// sheaths, which are bones and nodes of their own.
inline bool IsBodySlotEntry(const char* entry, unsigned length) {
	static const char* const kSlots[] = {"Head",     "Hair",      "UpperBody", "LowerBody", "Hand", "Foot",
	                                     "RightRing", "LeftRing", "Amulet",    "Shield",    "Tail"};
	for (const char* slot : kSlots) {
		unsigned at = 0;
		while (at < length && slot[at] != '\0' && LowerAscii(slot[at]) == LowerAscii(entry[at])) {
			++at;
		}
		if (at == length && slot[at] == '\0') {
			return true;
		}
	}
	return false;
}

// Whether a part of the first-person model is hidden by the list. Under a
// worn item's node (`wornNode`, its nearest such ancestor, null for none) the
// item's slot decides first: the slot listed hides every part of it, whatever
// the part is called; and an entry that names another body slot does not
// reach it. A part's own name can be anything a modder chose - the male hand
// of the testers' body is "Foot:handRM" (characters\_male\hand.nif, both
// installs, 2026-11-10), which the boots' "Foot" took for a foot: a male
// character had no first-person hands in Full VR (Nadi, 2026-11-10, right
// after the race menu's Done). Entries that are not slots (a bone, an arm
// mesh, "Weapon") match by the part's own name as before.
inline bool ShapeHiddenByList(const char* name, const char* wornNode, const char* list) {
	if (name == nullptr || list == nullptr || name[0] == '\0') {
		return false;
	}
	const unsigned slot = WornSlotLength(wornNode);
	if (slot == 0) {
		return NameInList(name, list);
	}
	const char* at = list;
	while (*at != '\0') {
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
		const char* end = at;
		while (end > entry && (end[-1] == ' ' || end[-1] == '\t')) {
			--end;
		}
		const unsigned length = static_cast<unsigned>(end - entry);
		if (length == 0) {
			continue;
		}
		if (IsBodySlotEntry(entry, length)) {
			// A slot: this part's own slot listed hides it; another slot's
			// name says nothing about it.
			unsigned same = 0;
			while (same < length && same < slot && LowerAscii(entry[same]) == LowerAscii(wornNode[same])) {
				++same;
			}
			if (same == length && same == slot) {
				return true;
			}
			continue;
		}
		char one[96];
		if (length < sizeof(one)) {
			for (unsigned i = 0; i < length; ++i) {
				one[i] = entry[i];
			}
			one[length] = '\0';
			if (NameInList(name, one)) {
				return true;
			}
		}
	}
	return false;
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
