#pragma once

#include "core/Types.h"

namespace obvr {

// A choice setting's word from the INI, any case: the index of the name it
// is. False, and `index` left alone, for anything else. Pure; covered by
// the tests of each choice (walk_direction_test, teleport_sound_test).
inline bool MatchChoiceWord(const char* text, const char* const* names, UInt32 count, UInt32& index) {
	if (text == nullptr) {
		return false;
	}
	for (UInt32 i = 0; i < count; ++i) {
		const char* a = text;
		const char* b = names[i];
		while (*a != '\0' && *b != '\0' && (*a == *b || *a + ('a' - 'A') == *b)) {
			++a;
			++b;
		}
		if (*a == '\0' && *b == '\0') {
			index = i;
			return true;
		}
	}
	return false;
}

}  // namespace obvr
