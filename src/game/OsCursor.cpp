#include "game/OsCursor.h"

#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/IniSettings.h"
#include "game/MenuCursor.h"
#include "game/MenuMode.h"
#include "game/MenuType.h"
#include "game/OsCursorLogic.h"
#include "platform/ImportHook.h"
#include "platform/Win32Min.h"

namespace obvr::game {

namespace {

// user32's POINT: two LONGs.
struct CursorPoint {
	long x;
	long y;
};

using GetCursorPosFn = int(__stdcall*)(CursorPoint*);

// The one owner of this file's state (the size ratchet's rule).
struct OsCursorState {
	GetCursorPosFn real = nullptr;
	UInt32 askedHeight = 0;  // iSize H as the settings hold it; 0 until read
	bool askedRead = false;
	UInt32 answersLogged = 0;
};

OsCursorState& State() {
	static OsCursorState state;
	return state;
}

// "iSize H" or "iSize H:Display", as game/IniSettings.cpp spells the match.
bool IsSizeHeightName(const char* name) {
	const char* wanted = "iSize H";
	if (name == nullptr) {
		return false;
	}
	while (*wanted != '\0' && *name == *wanted) {
		++name;
		++wanted;
	}
	return *wanted == '\0' && (*name == '\0' || *name == ':');
}

// The height the game asked for: the setting itself, which keeps the INI's
// number while the interface's copy is raised (render/UiScreenSize.h). Read
// once; the walk is bounded like game::OverrideSizeSettings'.
UInt32 AskedHeight() {
	OsCursorState& state = State();
	if (state.askedRead) {
		return state.askedHeight;
	}
	state.askedRead = true;
	IniSettingEntry* entry = ResolveIniSettingsList();
	for (int steps = 0; entry != nullptr && steps < 100000; ++steps, entry = entry->next) {
		const IniSettingInfo* data = entry->data;
		if (data != nullptr && IsSizeHeightName(data->name) && data->i > 0 && data->i < 65536) {
			state.askedHeight = static_cast<UInt32>(data->i);
			break;
		}
	}
	OBVR_LOG("OS cursor: iSize H reads %u (0 = not found - the believed height is used)", state.askedHeight);
	return state.askedHeight;
}

int __stdcall AnswerGetCursorPos(CursorPoint* point) {
	if (point != nullptr) {
		OsCursorAsk ask;
		ask.menuMode = IsMenuMode();
		if (ask.menuMode) {
			// The engine's own cursor first: a drag (a slider's knob) moves
			// with it alone (game/MenuCursor.h); else the floats as they are.
			ask.cursorKnown = EngineMenuCursor(ask.x, ask.y) || InterfaceCursorPosition(ask.x, ask.y);
		}
		ask.askedHeight = ask.menuMode ? AskedHeight() : 0;
		ask.believedHeight = *reinterpret_cast<const UInt32*>(addr::kUiScreenHeightCopy);
		const OsCursorAnswer answer = AnswerOsCursor(ask);
		if (answer.replace) {
			point->x = answer.x;
			point->y = answer.y;
			if (State().answersLogged < 3) {
				++State().answersLogged;
				OBVR_LOG("OS cursor: a script's GetMousePos answered with the interface cursor %ld,%ld (believed %.1f,%.1f, "
				         "asked height %u, believed %u)",
				         point->x, point->y, ask.x, ask.y, ask.askedHeight, ask.believedHeight);
			}
			return 1;
		}
	}
	return State().real != nullptr ? State().real(point) : 0;
}

}  // namespace

bool InstallOsCursorAnswer() {
	const char* const obseNames[] = {"obse_1_2_416.dll", "obse_steam_loader.dll"};
	for (const char* name : obseNames) {
		const HMODULE module = GetModuleHandleA(name);
		if (module == nullptr) {
			continue;
		}
		void* previous = nullptr;
		if (platform::ReplaceImport(reinterpret_cast<UInt32>(module), "USER32.dll", "GetCursorPos",
		                            reinterpret_cast<void*>(&AnswerGetCursorPos), &previous) &&
		    previous != nullptr) {
			State().real = reinterpret_cast<GetCursorPosFn>(previous);
			OBVR_LOG("OS cursor: %s's GetCursorPos goes through OBVR - a script's GetMousePos in a menu reads the "
			         "interface cursor (the laser's, the drag's), not the desktop's mouse",
			         name);
			return true;
		}
	}
	OBVR_LOG("OS cursor: xOBSE's GetCursorPos import not found - a script's GetMousePos reads the desktop's mouse");
	return false;
}

}  // namespace obvr::game
