#include "core/GameIni.h"

#include "platform/Win32Min.h"

namespace obvr::core {
namespace {

// Appends to a buffer, refusing rather than truncating.
//
// Truncation here would produce a path that exists nowhere and a write that
// silently goes to it, which is a setting that appears to have been applied
// and was not.
bool Append(char* out, UInt32 outSize, UInt32& used, const char* text) {
	while (*text != '\0') {
		if (used + 1 >= outSize) {
			return false;
		}
		out[used++] = *text++;
	}
	out[used] = '\0';
	return true;
}

UInt32 ParseUInt(const char* text) {
	UInt32 value = 0;
	for (const char* p = text; *p >= '0' && *p <= '9'; ++p) {
		value = value * 10 + static_cast<UInt32>(*p - '0');
	}
	return value;
}

void FormatUInt(UInt32 value, char* out) {
	char digits[12];
	int count = 0;
	if (value == 0) {
		digits[count++] = '0';
	}
	while (value > 0 && count < 11) {
		digits[count++] = static_cast<char>('0' + (value % 10));
		value /= 10;
	}
	for (int i = 0; i < count; ++i) {
		out[i] = digits[count - 1 - i];
	}
	out[count] = '\0';
}

}  // namespace

bool FindOblivionIni(char* out, UInt32 outSize) {
	if (out == nullptr || outSize == 0) {
		return false;
	}

	// USERPROFILE rather than the shell's own folder lookup, which would mean
	// importing shell32 for one string. This is wrong for a Documents folder
	// that has been redirected, and the log says which path was tried so that
	// case is visible rather than mysterious.
	char profile[260];
	const DWORD length = GetEnvironmentVariableA("USERPROFILE", profile, sizeof(profile));
	if (length == 0 || length >= sizeof(profile)) {
		return false;
	}

	UInt32 used = 0;
	out[0] = '\0';
	return Append(out, outSize, used, profile) &&
	       Append(out, outSize, used, "\Documents\My Games\Oblivion\Oblivion.ini");
}

bool ReadRenderSize(const char* path, UInt32& width, UInt32& height) {
	char buffer[32];

	// "iSize W" and "iSize H", spelled with the space Bethesda used.
	if (GetPrivateProfileStringA("Display", "iSize W", "", buffer, sizeof(buffer), path) == 0) {
		return false;
	}
	const UInt32 w = ParseUInt(buffer);

	if (GetPrivateProfileStringA("Display", "iSize H", "", buffer, sizeof(buffer), path) == 0) {
		return false;
	}
	const UInt32 h = ParseUInt(buffer);

	if (w == 0 || h == 0) {
		return false;
	}

	width = w;
	height = h;
	return true;
}

bool WriteRenderSize(const char* path, UInt32 width, UInt32 height) {
	if (width == 0 || height == 0) {
		return false;
	}

	char text[12];
	FormatUInt(width, text);
	if (!WritePrivateProfileStringA("Display", "iSize W", text, path)) {
		return false;
	}

	FormatUInt(height, text);
	return WritePrivateProfileStringA("Display", "iSize H", text, path) != 0;
}

}  // namespace obvr::core
