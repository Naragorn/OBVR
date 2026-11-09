#pragma once

// The engine's message boxes in the log (docs/container-touch-spec.md 8.3):
// a merchant's purchase ended in a box whose text and buttons no log said,
// so the bought item's fate could not be told. The box builder
// (0x005BC8B0, game/MessageBoxTrace.cpp) takes the text, the callback, a flag, the first
// button's label and a pointer to the rest of the caller's labels, ended by
// a null; these gather the labels and write the one line.
//
// Pure: covered by message_box_logic_test.

#include <cstdio>

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kMessageBoxButtonsMost = 8;

// The labels: the first, then the rest until their null (or an empty label,
// where the builder stops as well) or the capacity.
// A null first label is a box without buttons (the engine shows none).
inline UInt32 CollectMessageBoxButtons(const char* first, const char* const* rest, const char** out,
                                       UInt32 capacity) {
	if (out == nullptr || capacity == 0 || first == nullptr) {
		return 0;
	}
	UInt32 count = 0;
	out[count++] = first;
	for (UInt32 i = 0; rest != nullptr && count < capacity && rest[i] != nullptr && rest[i][0] != '\0'; ++i) {
		out[count++] = rest[i];
	}
	return count;
}

// "the text" [button | button]; an empty or missing text is said as such.
// Truncated to the buffer, always ended. Answers the characters written.
inline UInt32 ComposeMessageBoxLine(char* out, UInt32 capacity, const char* text, const char* const* buttons,
                                    UInt32 count) {
	if (out == nullptr || capacity == 0) {
		return 0;
	}
	out[0] = '\0';
	const char* said = text == nullptr ? "(no text)" : text[0] == '\0' ? "(empty text)" : text;
	int used = std::snprintf(out, capacity, "\"%s\" [", said);
	for (UInt32 i = 0; i < count && used >= 0 && static_cast<UInt32>(used) < capacity; ++i) {
		const char* label = buttons[i] != nullptr ? buttons[i] : "";
		used += std::snprintf(out + used, capacity - static_cast<UInt32>(used), "%s%s", i > 0 ? " | " : "", label);
	}
	if (used >= 0 && static_cast<UInt32>(used) < capacity) {
		used += std::snprintf(out + used, capacity - static_cast<UInt32>(used), "%s", count == 0 ? "no buttons]" : "]");
	}
	if (used < 0) {
		out[0] = '\0';
		return 0;
	}
	return static_cast<UInt32>(used) < capacity ? static_cast<UInt32>(used) : capacity - 1;
}

}  // namespace obvr::game
