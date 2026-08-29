#include "game/IniSettings.h"

#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

// Oblivion.ini holds a few hundred settings; a list past this length is not
// that list.
constexpr UInt32 kMaxWalk = 4096;

// Exact match, deliberately case-sensitive where xOBSE compares loosely.
// This code writes into engine memory on the strength of the comparison, and
// stricter is safer: the engine's own names are fixed strings in the binary,
// so the exact spelling is known.
bool Same(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return false;
	}
	while (*a != '\0' && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

}  // namespace

bool OverrideSizeSettings(IniSettingEntry* first, UInt32 expectedWidth,
                          UInt32 expectedHeight, UInt32 newWidth, UInt32 newHeight) {
	IniSettingInfo* width = nullptr;
	IniSettingInfo* height = nullptr;

	UInt32 walked = 0;
	for (IniSettingEntry* entry = first; entry != nullptr && walked < kMaxWalk;
	     entry = entry->next, ++walked) {
		IniSettingInfo* data = entry->data;
		if (data == nullptr || data->name == nullptr) {
			continue;
		}
		if (Same(data->name, "iSize W")) {
			width = data;
		} else if (Same(data->name, "iSize H")) {
			height = data;
		}
	}

	// Both checked before either is written: a screen half one size and half
	// the other would be the very split this exists to end.
	if (width == nullptr || height == nullptr) {
		OBVR_LOG("IniSettings: iSize %s not found in the setting list, so the game keeps "
		         "its own screen size",
		         width == nullptr ? (height == nullptr ? "W and iSize H" : "W") : "H");
		return false;
	}

	if (width->i != static_cast<int>(expectedWidth) ||
	    height->i != static_cast<int>(expectedHeight)) {
		OBVR_LOG("IniSettings: iSize reads %dx%d where the game just asked for %ux%u - "
		         "the list is not what it is believed to be, so nothing was written",
		         width->i, height->i, expectedWidth, expectedHeight);
		return false;
	}

	width->i = static_cast<int>(newWidth);
	height->i = static_cast<int>(newHeight);
	return true;
}

IniSettingEntry* ResolveIniSettingsList() {
	auto* base = reinterpret_cast<const UInt8*>(addr::kIniSettingCollection);

	// The vtable of a static engine object points into the executable image.
	// Anything else at this address is not the collection.
	const UInt32 vtable = *reinterpret_cast<const UInt32*>(base);
	if (vtable < 0x00400000 || vtable >= 0x00C00000) {
		OBVR_LOG("IniSettings: 0x%08X does not hold the setting collection (vtable reads "
		         "%08X), so the game keeps its own screen size",
		         addr::kIniSettingCollection, vtable);
		return nullptr;
	}

	// The path field: printable characters ending in .ini within its 0x104
	// bytes. The check is what makes a coincidentally plausible vtable value
	// insufficient on its own.
	const char* path = reinterpret_cast<const char*>(base + 4);
	bool pathLooksRight = false;
	for (UInt32 index = 0; index < 0x104; ++index) {
		const char character = path[index];
		if (character == '\0') {
			pathLooksRight = index >= 4 && (path[index - 4] == '.') &&
			                 (path[index - 3] == 'i' || path[index - 3] == 'I') &&
			                 (path[index - 2] == 'n' || path[index - 2] == 'N') &&
			                 (path[index - 1] == 'i' || path[index - 1] == 'I');
			break;
		}
		// Only control characters disqualify; bytes past 0x7F are letters in
		// some codepage and a path is allowed to contain them.
		if (static_cast<unsigned char>(character) < 0x20) {
			break;
		}
	}
	if (!pathLooksRight) {
		OBVR_LOG("IniSettings: the collection at 0x%08X does not carry an .ini path, so "
		         "the game keeps its own screen size",
		         addr::kIniSettingCollection);
		return nullptr;
	}

	// The first entry lives inline in the object; the rest hang off next.
	return reinterpret_cast<IniSettingEntry*>(
		const_cast<UInt8*>(base) + addr::kIniSettingListOffset);
}

}  // namespace obvr::game
