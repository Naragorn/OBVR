#pragma once

// Whether Steam's overlay is switched on for the game that is running, read
// from Steam's own settings - not from GameOverlayRenderer.dll being loaded.
//
// Steam loads GameOverlayRenderer.dll into every game it starts, overlay on
// or off: switching the overlay off stops its UI process and most of the
// DLL's logic, but the DLL stays for Steam Input, streaming and the rest
// (a Valve developer, Steam Client Beta forum, 2024-02-07,
// steamcommunity.com/groups/SteamClientBeta/discussions/3/4206993388791543748;
// the same reported in ValveSoftware/halflife#2928). Oblivion's Steam exe
// carries the Steam stub (its .bind section), so a start from obse_loader.exe
// outside Steam is handed to Steam as app 22330 and gets the DLL as well -
// measured 2026-11-12/13 on the Windows install: the DLL in the crash log's
// module list, obse_steam_loader.log written in the same minute as
// obse_loader.log, while app 22330 has "OverlayAppEnable" "0".
//
// The settings, as Special K's SKIF_Steam_isSteamOverlayEnabled reads them
// (github.com/SpecialKO/SKIF, src/stores/Steam/steam_library.cpp):
//   userdata/<user>/config/localconfig.vdf (text KeyValues)
//     UserLocalConfigStore/system/EnableGameOverlay "0"   off for all games
//     UserLocalConfigStore/apps/<appid>/OverlayAppEnable "0"  off for one
//   userdata/<user>/config/shortcuts.vdf (binary KeyValues), for a non-Steam
//     shortcut: shortcuts/<n>/appid (int32) and AllowOverlay (int32, 0 = off).
// A non-Steam shortcut's game id is (appid << 32) | 0x02000000; Steam hands
// the running game its id in the SteamGameId environment variable.

#include "core/Types.h"

namespace obvr::selfcheck {

enum class OverlaySetting : UInt8 {
	Unread,        // the settings could not be read
	On,            // on for the game (Steam's default when nothing says off)
	OffForGame,    // apps/<id>/OverlayAppEnable "0"
	OffForAll,     // system/EnableGameOverlay "0"
	ShortcutOn,    // the non-Steam shortcut it was started from: AllowOverlay 1
	ShortcutOff,   // ... AllowOverlay 0
};

inline const char* OverlaySettingName(OverlaySetting s) {
	switch (s) {
	case OverlaySetting::Unread: return "unread";
	case OverlaySetting::On: return "on";
	case OverlaySetting::OffForGame: return "off for the game";
	case OverlaySetting::OffForAll: return "off for all games";
	case OverlaySetting::ShortcutOn: return "on for the shortcut";
	case OverlaySetting::ShortcutOff: return "off for the shortcut";
	}
	return "?";
}

constexpr UInt32 kOblivionAppId = 22330;

namespace steamcfg_detail {

inline char Lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

inline bool SameNoCase(const char* a, UInt32 aLen, const char* b) {
	UInt32 i = 0;
	for (; i < aLen; ++i) {
		if (b[i] == '\0' || Lower(a[i]) != Lower(b[i])) return false;
	}
	return b[i] == '\0';
}

inline bool IsDecimal(const char* s, UInt32 len, UInt32 value) {
	if (len == 0 || len > 10) return false;
	UInt64 v = 0;
	for (UInt32 i = 0; i < len; ++i) {
		if (s[i] < '0' || s[i] > '9') return false;
		v = v * 10 + static_cast<UInt64>(s[i] - '0');
	}
	return v == value;
}

}  // namespace steamcfg_detail

// The game id Steam put in SteamGameId ("22330" or a shortcut's 64-bit id);
// 0 when the text is empty or not a number.
inline UInt64 ParseSteamGameId(const char* text) {
	if (text == nullptr || *text == '\0') return 0;
	UInt64 v = 0;
	for (const char* p = text; *p != '\0'; ++p) {
		if (*p < '0' || *p > '9') return 0;
		v = v * 10 + static_cast<UInt64>(*p - '0');
	}
	return v;
}

// A shortcut's game id carries its 32-bit app id in the high half.
inline bool IsShortcutGameId(UInt64 gameId) { return (gameId >> 32) != 0; }
inline UInt32 ShortcutAppIdOf(UInt64 gameId) { return static_cast<UInt32>(gameId >> 32); }

// localconfig.vdf's text: OffForAll, OffForGame for `appId`, else On (Steam's
// default). The keys compare without case, as Steam's KeyValues do. Quoted
// tokens with \" and \\ escapes, braces, and // comments are understood.
inline OverlaySetting LocalConfigOverlay(const char* text, UInt32 len, UInt32 appId) {
	using namespace steamcfg_detail;
	constexpr UInt32 kDepth = 8;
	const char* keys[kDepth] = {};
	UInt32 keyLens[kDepth] = {};
	UInt32 depth = 0;
	const char* pending = nullptr;  // a key read, waiting for its value or a brace
	UInt32 pendingLen = 0;
	bool offAll = false;
	bool offGame = false;
	UInt32 i = 0;
	auto inUserStore = [&](UInt32 d) { return d >= 1 && SameNoCase(keys[0], keyLens[0], "UserLocalConfigStore") && depth == d; };
	while (i < len) {
		const char c = text[i];
		if (c == '/' && i + 1 < len && text[i + 1] == '/') {
			while (i < len && text[i] != '\n') ++i;
			continue;
		}
		if (c == '{') {
			if (depth < kDepth) {
				keys[depth] = pending;
				keyLens[depth] = pendingLen;
			}
			++depth;
			pending = nullptr;
			++i;
			continue;
		}
		if (c == '}') {
			if (depth > 0) --depth;
			pending = nullptr;
			++i;
			continue;
		}
		if (c == '"') {
			const UInt32 start = ++i;
			while (i < len && text[i] != '"') {
				i += (text[i] == '\\' && i + 1 < len) ? 2 : 1;
			}
			const UInt32 tokenLen = (i < len ? i : len) - start;
			if (i < len) ++i;
			const char* token = text + start;
			if (pending == nullptr) {
				pending = token;
				pendingLen = tokenLen;
				continue;
			}
			// pending = key, token = value, at the current depth.
			if (depth == 2 && depth <= kDepth && inUserStore(2) && SameNoCase(keys[1], keyLens[1], "system") &&
			    SameNoCase(pending, pendingLen, "EnableGameOverlay") && tokenLen == 1 && token[0] == '0') {
				offAll = true;
			}
			if (depth == 3 && inUserStore(3) && SameNoCase(keys[1], keyLens[1], "apps") &&
			    IsDecimal(keys[2], keyLens[2], appId) && SameNoCase(pending, pendingLen, "OverlayAppEnable") &&
			    tokenLen == 1 && token[0] == '0') {
				offGame = true;
			}
			pending = nullptr;
			continue;
		}
		++i;
	}
	if (offAll) return OverlaySetting::OffForAll;
	if (offGame) return OverlaySetting::OffForGame;
	return OverlaySetting::On;
}

// shortcuts.vdf's binary KeyValues: 0 for AllowOverlay 0, 1 for on (also
// when the shortcut has no AllowOverlay - Steam's default), -1 when no
// shortcut has `appId` or the data does not parse. Entries: a type byte
// (0 map, 1 string, 2 int32, 8 end of map), a NUL-terminated name, the value.
inline int ShortcutAllowsOverlay(const UInt8* data, UInt32 len, UInt32 appId) {
	using namespace steamcfg_detail;
	constexpr UInt32 kDepth = 8;
	UInt32 depth = 0;
	// Per shortcut map (depth 2: root "shortcuts" is depth 1).
	bool haveId = false;
	UInt32 id = 0;
	int allow = 1;
	UInt32 i = 0;
	while (i < len) {
		const UInt8 type = data[i++];
		if (type == 8) {
			if (depth == 0) return -1;
			if (depth == 2 && haveId && id == appId) return allow;
			--depth;
			if (depth == 1) {
				haveId = false;
				allow = 1;
			}
			if (depth == 0) return -1;
			continue;
		}
		const char* name = reinterpret_cast<const char*>(data + i);
		UInt32 nameLen = 0;
		while (i < len && data[i] != 0) {
			++i;
			++nameLen;
		}
		if (i >= len) return -1;
		++i;  // the NUL
		if (type == 0) {
			if (++depth > kDepth) return -1;
			continue;
		}
		if (type == 1) {
			while (i < len && data[i] != 0) ++i;
			if (i >= len) return -1;
			++i;
			continue;
		}
		if (type == 2) {
			if (i + 4 > len) return -1;
			const UInt32 v = static_cast<UInt32>(data[i]) | (static_cast<UInt32>(data[i + 1]) << 8) |
			                 (static_cast<UInt32>(data[i + 2]) << 16) | (static_cast<UInt32>(data[i + 3]) << 24);
			i += 4;
			if (depth == 2) {
				if (SameNoCase(name, nameLen, "appid")) {
					haveId = true;
					id = v;
				} else if (SameNoCase(name, nameLen, "AllowOverlay")) {
					allow = v != 0 ? 1 : 0;
				}
			}
			continue;
		}
		if (type == 3 || type == 7) {  // float32, uint64: skipped by their size
			const UInt32 size = type == 3 ? 4u : 8u;
			if (i + size > len) return -1;
			i += size;
			continue;
		}
		return -1;  // a type this reader does not know: stop rather than misread
	}
	return -1;
}

}  // namespace obvr::selfcheck
