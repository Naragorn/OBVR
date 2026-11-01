#pragma once

// The INI's keys as a table (docs/god-functions-plan.md, phase 3: Config's
// ReadRuntimeValues table-driven): one row per key with its section, its
// kind and the Config member it lands in, in the order the keys are read -
// the order matters where a later key's fallback is an earlier one's value
// (VerticalLookRange before VerticalLookUpRange) and where a step between
// rows derives or clamps what was read. Those steps are rows too
// (IniKind::Step), so the table is the whole read, in order.
//
// The table is data for the tests: every key must be read into a member of
// its kind, no two rows may name one key unless meant (the legacy
// VerticalLookRange pair), and a key set in an INI must move its member.

#include <utility>

#include "core/Config.h"

namespace obvr::config {

enum class IniKind : UInt8 {
	Float,           // ReadFloat
	Bool,            // ReadBool
	UInt,            // ReadUInt
	Int,             // ReadUInt through an int member
	KeyCode,         // ReadKeyCode (decimal or 0x hex, 0 to 255)
	HexColor,        // ReadHexColor (RRGGBB)
	AnchorIsWorld,   // "world"/"room" true, "head"/"hmd" false
	MenusInWorld,    // "world"/"room" true, "cinema"/"screen"/"flat" false
	LegacyWordBool,  // falseWord/trueWord, else ReadBool
	Text,            // ReadText into a char array of `size`
	Step,            // no key: a block of its own between the rows
};

struct IniKey {
	const char* section;
	const char* key;  // a step's name for Step rows
	IniKind kind;
	void* (*at)(Config& config);  // the member, null for a step
	UInt32 size;                  // Text: the array's capacity
	const char* falseWord;        // LegacyWordBool
	const char* trueWord;
	void (*step)(Config& config, const char* path);  // Step
};

// The table and its length.
const IniKey* ConfigKeys(UInt32& count);

}  // namespace obvr::config
