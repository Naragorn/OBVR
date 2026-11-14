#pragma once

// A scene node's name read with care, and the two string tests the body's
// bone walks use (game/BodyIk.cpp): no CRT, the name checked before it is read.

#include "core/AddressSpace.h"
#include "game/GameTypes.h"

namespace obvr::game::node_name {

// The node's name, or "" when the node or its name does not look like memory
// one can read, or the name runs past 64 characters or holds a non-printable one.
inline const char* SafeName(const NiAVObject* node) {
	if (!mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node)) ||
	    !mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node->name))) {
		return "";
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const char c = node->name[at];
		if (c == '\0') {
			return node->name;
		}
		if (c < 0x20 || c > 0x7E) {
			return "";
		}
	}
	return "";
}

inline bool StartsWith(const char* text, const char* prefix) {
	for (; *prefix != '\0'; ++text, ++prefix) {
		if (*text != *prefix) {
			return false;
		}
	}
	return true;
}

inline bool Is(const char* a, const char* b) {
	for (; *a != '\0' && *a == *b; ++a, ++b) {
	}
	return *a == *b;
}

}  // namespace obvr::game::node_name
