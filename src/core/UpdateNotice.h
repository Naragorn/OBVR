#pragma once

#include "core/Types.h"

// Whether a newer OBVR has been released, and when to say so.
//
// The network side (platform/UpdateFetch) asks GitHub for the latest release
// once per start and hands back the release's tag. Everything that decides
// something about it lives here, over plain values: reading the tag out of
// the answer, comparing it with the running version, and the notice's own
// lifetime - shown in the main menu, kept for the first thirty seconds in the
// world, then gone for the rest of the session.
namespace obvr::update {

// How long the notice stays once the player is in the world.
constexpr float kNoticeWorldSeconds = 30.0f;

// "v0.2.3" or "0.2.3" into up to four numbers; missing ones are zero. Refuses
// anything else - an empty string, a letter, a fifth number, a number that
// does not fit - because a tag that cannot be read must not look newer.
inline bool ParseVersion(const char* text, UInt32 (&parts)[4]) {
	for (UInt32& part : parts) {
		part = 0;
	}
	if (text == nullptr) {
		return false;
	}
	if (*text == 'v' || *text == 'V') {
		++text;
	}
	UInt32 index = 0;
	bool digits = false;
	for (;; ++text) {
		const char c = *text;
		if (c >= '0' && c <= '9') {
			if (parts[index] > 100000u) {
				return false;
			}
			parts[index] = parts[index] * 10u + static_cast<UInt32>(c - '0');
			digits = true;
		} else if (c == '.' && digits && index < 3) {
			++index;
			digits = false;
		} else if (c == '\0' && digits) {
			return true;
		} else {
			return false;
		}
	}
}

// True only when both read as versions and the first is the later one.
inline bool IsNewerVersion(const char* latest, const char* running) {
	UInt32 a[4];
	UInt32 b[4];
	if (!ParseVersion(latest, a) || !ParseVersion(running, b)) {
		return false;
	}
	for (UInt32 i = 0; i < 4; ++i) {
		if (a[i] != b[i]) {
			return a[i] > b[i];
		}
	}
	return false;
}

// The value of "tag_name" in the release answer, without its quotes. Refuses
// a missing key, a value that is not a plain string (an escape included), and
// one that does not fit, rather than hand back half a tag.
inline bool ExtractTagName(const char* json, UInt32 length, char* out, UInt32 capacity) {
	if (out == nullptr || capacity == 0) {
		return false;
	}
	out[0] = '\0';
	if (json == nullptr) {
		return false;
	}
	static const char kKey[] = "\"tag_name\"";
	const UInt32 keyLength = sizeof(kKey) - 1;
	for (UInt32 at = 0; at + keyLength <= length; ++at) {
		UInt32 k = 0;
		while (k < keyLength && json[at + k] == kKey[k]) {
			++k;
		}
		if (k != keyLength) {
			continue;
		}
		UInt32 p = at + keyLength;
		while (p < length && (json[p] == ' ' || json[p] == '\t' || json[p] == '\r' || json[p] == '\n')) {
			++p;
		}
		if (p >= length || json[p] != ':') {
			return false;
		}
		++p;
		while (p < length && (json[p] == ' ' || json[p] == '\t' || json[p] == '\r' || json[p] == '\n')) {
			++p;
		}
		if (p >= length || json[p] != '"') {
			return false;
		}
		++p;
		UInt32 written = 0;
		for (; p < length && json[p] != '"'; ++p) {
			if (json[p] == '\\' || written + 1 >= capacity) {
				out[0] = '\0';
				return false;
			}
			out[written++] = json[p];
		}
		if (p >= length || written == 0) {
			out[0] = '\0';
			return false;
		}
		out[written] = '\0';
		return true;
	}
	return false;
}

// The line the notice shows: "An OBVR update is available! 0.2.3" - the tag
// without its leading v. False, and an empty line, when it does not fit.
inline bool FormatNotice(const char* tag, char* out, UInt32 capacity) {
	if (out == nullptr || capacity == 0) {
		return false;
	}
	out[0] = '\0';
	if (tag == nullptr) {
		return false;
	}
	if (*tag == 'v' || *tag == 'V') {
		++tag;
	}
	static const char kLead[] = "An OBVR update is available! ";
	const char* const parts[] = {kLead, tag};
	UInt32 at = 0;
	for (const char* part : parts) {
		for (; *part != '\0'; ++part) {
			if (at + 1 >= capacity) {
				out[0] = '\0';
				return false;
			}
			out[at++] = *part;
		}
	}
	out[at] = '\0';
	return true;
}

struct NoticeState {
	bool everInWorld = false;
	float worldSeconds = 0.0f;
};

// Once per frame. Whether the notice is up: only when an update is known, and
// then until the player has spent kNoticeWorldSeconds in the world. Time in the
// main menu or on a loading screen does not count; going back to the main menu
// afterwards does not bring the notice back.
inline bool StepNotice(NoticeState& state, bool updateAvailable, bool inWorld, float deltaSeconds) {
	if (inWorld) {
		state.everInWorld = true;
		if (deltaSeconds > 0.0f) {
			state.worldSeconds += deltaSeconds;
		}
	}
	return updateAvailable && (!state.everInWorld || state.worldSeconds < kNoticeWorldSeconds);
}

}  // namespace obvr::update
