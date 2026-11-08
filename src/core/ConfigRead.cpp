#include "core/ConfigRead.h"

#include <cstdio>

#include "core/Log.h"
#include "render/MenuShade.h"
#include "platform/Win32Min.h"

namespace obvr::config_read {

// A case insensitive comparison of our own: the CRT variants go by different
// names depending on the environment, and this is only about a handful of
// keywords.
bool EqualsIgnoreCase(const char* a, const char* b) {
	while (*a != '\0' && *b != '\0') {
		char ca = *a;
		char cb = *b;
		if (ca >= 'A' && ca <= 'Z') {
			ca = static_cast<char>(ca - 'A' + 'a');
		}
		if (cb >= 'A' && cb <= 'Z') {
			cb = static_cast<char>(cb - 'A' + 'a');
		}
		if (ca != cb) {
			return false;
		}
		++a;
		++b;
	}
	return *a == *b;
}

float ReadFloat(const char* section, const char* key, float fallback, const char* path) {
	char buffer[64];
	if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}
	return static_cast<float>(atof(buffer));
}

// A line of text as it stands, into a fixed buffer; the fallback already in
// the buffer is kept when the key is absent. An empty value is a value - it
// is how a list is cleared.
void ReadText(const char* section, const char* key, char* buffer, UInt32 capacity,
              const char* path) {
	// The count of characters copied cannot tell an absent key from a present
	// empty one - both copy nothing - so the default is a sentinel no INI
	// line carries, and absence is its coming back.
	char read[256];
	GetPrivateProfileStringA(section, key, "\x01", read, sizeof(read), path);
	if (read[0] == '\x01') {
		return;
	}
	UInt32 at = 0;
	for (; at + 1 < capacity && read[at] != '\0'; ++at) {
		buffer[at] = read[at];
	}
	buffer[at] = '\0';
}

UInt32 ReadUInt(const char* section, const char* key, UInt32 fallback, const char* path) {
	char buffer[64];
	const DWORD length = GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path);
	if (length == 0) {
		return fallback;
	}

	UInt32 value = 0;
	for (DWORD i = 0; i < length; ++i) {
		if (buffer[i] < '0' || buffer[i] > '9') {
			return fallback;
		}
		value = value * 10 + static_cast<UInt32>(buffer[i] - '0');
	}
	return value;
}

bool ReadBool(const char* section, const char* key, bool fallback, const char* path) {
	return ReadUInt(section, key, fallback ? 1 : 0, path) != 0;
}

// Four toggle rows briefly wrote descriptive words even though their reader
// was numeric.  Accept those already-written values while the settings menu
// goes back to writing the canonical 0/1 form.  Keeping this narrow matters:
// an arbitrary word in an ordinary boolean must remain a rejected typo, not
// silently become true.
bool ReadLegacyWordBool(const char* section, const char* key, bool fallback,
                        const char* falseWord, const char* trueWord, const char* path) {
	char buffer[64];
	if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}
	if (EqualsIgnoreCase(buffer, falseWord)) {
		return false;
	}
	if (EqualsIgnoreCase(buffer, trueWord)) {
		return true;
	}
	return ReadBool(section, key, fallback, path);
}

// Reads a virtual-key code. Accepts decimal (46) as well as hex with an 0x
// prefix (0x2E).
//
// The hex form matters: Microsoft's virtual-key table lists the codes in hex,
// so that is what people will copy. ReadUInt would quietly fall back on such
// a value, and the key would then simply appear not to work.
UInt32 ReadKeyCode(const char* section, const char* key, UInt32 fallback, const char* path) {
	char buffer[64];
	const DWORD length = GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path);
	if (length == 0) {
		return fallback;
	}

	DWORD i = 0;
	UInt32 base = 10;
	if (length > 2 && buffer[0] == '0' && (buffer[1] == 'x' || buffer[1] == 'X')) {
		base = 16;
		i = 2;
	}

	UInt32 value = 0;
	for (; i < length; ++i) {
		const char c = buffer[i];
		UInt32 digit = 0;

		if (c >= '0' && c <= '9') {
			digit = static_cast<UInt32>(c - '0');
		} else if (base == 16 && c >= 'a' && c <= 'f') {
			digit = static_cast<UInt32>(c - 'a' + 10);
		} else if (base == 16 && c >= 'A' && c <= 'F') {
			digit = static_cast<UInt32>(c - 'A' + 10);
		} else {
			OBVR_LOG("Config: %s.%s=\"%s\" is not a number, keeping the previous key",
			         section, key, buffer);
			return fallback;
		}

		value = value * base + digit;
	}

	// Virtual-key codes fit in a byte. Anything larger is a typo, and polling
	// a key that cannot exist would look exactly like the key not working.
	if (value > 0xFF) {
		OBVR_LOG("Config: %s.%s=%u is not a virtual-key code (0 to 255), keeping the previous key",
		         section, key, value);
		return fallback;
	}

	return value;
}

vr::StereoMode ReadStereoMode(vr::StereoMode fallback, const char* path) {
	char buffer[32];
	if (GetPrivateProfileStringA("Render", "Stereo", "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}

	if (EqualsIgnoreCase(buffer, "none") || EqualsIgnoreCase(buffer, "off")) {
		return vr::StereoMode::None;
	}
	if (EqualsIgnoreCase(buffer, "aer") || EqualsIgnoreCase(buffer, "alternate")) {
		return vr::StereoMode::AlternateEyes;
	}

	if (EqualsIgnoreCase(buffer, "dual") || EqualsIgnoreCase(buffer, "dualpass")) {
		return vr::StereoMode::DualPass;
	}

	OBVR_LOG("Config: unknown Render.Stereo \"%s\", keeping the previous setting", buffer);
	return fallback;
}

// Reads HudAnchor, which names a place rather than answers a yes-or-no: the
// setting is "head" or "world", and a word is what a reader of the file will
// understand without looking the key up.
//
// An unrecognised word keeps the previous setting and says so, the same way
// Stereo does - silently defaulting would leave the HUD somewhere the person
// did not ask for with nothing in the log to explain it.
bool ReadAnchorIsWorld(const char* section, const char* key, bool fallback,
                       const char* path) {
	char buffer[32];
	if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}

	if (EqualsIgnoreCase(buffer, "head") || EqualsIgnoreCase(buffer, "hmd")) {
		return false;
	}
	if (EqualsIgnoreCase(buffer, "world") || EqualsIgnoreCase(buffer, "room")) {
		return true;
	}

	// The section is named as well as the key. This reader is used from more
	// than one section now, and "unknown Render.Anchor" for a setting in
	// [SettingsMenu] sends the reader to the wrong part of the file.
	OBVR_LOG("Config: unknown %s.%s \"%s\", keeping the previous setting", section, key, buffer);
	return fallback;
}

// Reads Menus, which names a place the same way HudAnchor does: "cinema" or
// "world". Kept as its own reader rather than folded into the one above,
// because the words are not the same ones - a menu is not on a head - and a
// reader who writes "head" here should be told, not quietly given the screen.
bool ReadMenusInWorld(const char* section, const char* key, bool fallback, const char* path) {
	char buffer[32];
	if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}

	if (EqualsIgnoreCase(buffer, "cinema") || EqualsIgnoreCase(buffer, "screen") ||
	    EqualsIgnoreCase(buffer, "flat")) {
		return false;
	}
	if (EqualsIgnoreCase(buffer, "world") || EqualsIgnoreCase(buffer, "room")) {
		return true;
	}

	OBVR_LOG("Config: unknown Render.%s \"%s\", keeping the previous setting", key, buffer);
	return fallback;
}

// Reads a colour written as six hex digits (RRGGBB, with or without '#').
// The parse itself lives in MenuShade.h with the rest of the shade logic; a
// value that does not parse keeps the previous colour and says so, the same
// way every other worded setting here behaves.
UInt32 ReadHexColor(const char* section, const char* key, UInt32 fallback, const char* path) {
	char buffer[32];
	if (GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path) == 0) {
		return fallback;
	}

	UInt32 rgb = 0;
	if (!render::ParseHexColor(buffer, rgb)) {
		OBVR_LOG("Config: %s.%s=\"%s\" is not an RRGGBB colour, keeping the previous one",
		         section, key, buffer);
		return fallback;
	}
	return rgb;
}

}  // namespace obvr::config_read
