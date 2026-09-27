#include "game/ControlBindings.h"

#include <cstdio>
#include <cstring>

#include "core/Log.h"
#include "game/KeyScanCodes.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// Where the game keeps its Oblivion.ini: My Games\Oblivion under the user's
// Documents, or beside Oblivion.exe when bUseMyGamesDirectory is off. The
// first that has a [Controls] Run line wins.
bool ReadRunLine(char* out, DWORD size, char* pathOut, DWORD pathSize) {
	char profile[260] = {};
	if (GetEnvironmentVariableA("USERPROFILE", profile, sizeof(profile)) != 0) {
		std::snprintf(pathOut, pathSize, "%s\\Documents\\My Games\\Oblivion\\Oblivion.ini", profile);
		if (GetPrivateProfileStringA("Controls", "Run", "", out, size, pathOut) != 0) {
			return true;
		}
	}
	char exe[260] = {};
	if (GetModuleFileNameA(nullptr, exe, sizeof(exe)) != 0) {
		char* const slash = std::strrchr(exe, '\\');
		if (slash != nullptr) {
			*slash = 0;
			std::snprintf(pathOut, pathSize, "%s\\Oblivion.ini", exe);
			if (GetPrivateProfileStringA("Controls", "Run", "", out, size, pathOut) != 0) {
				return true;
			}
		}
	}
	return false;
}

}  // namespace

void CheckDropBinding(UInt32 runKey) {
	static bool checked = false;
	if (checked) {
		return;
	}
	checked = true;
	char line[32] = {};
	char path[300] = {};
	const bool read = ReadRunLine(line, sizeof(line), path, sizeof(path));
	const ControlBinding run = read ? ParseControlBinding(line) : ControlBinding{};
	const UInt32 scan = UsScanCode(runKey);
	switch (JudgeDropBinding(run, static_cast<UInt8>(scan))) {
	case DropBindingVerdict::Fine:
		OBVR_LOG("Hands: the inventory drop (left A) presses Left Shift, which the game's Run is on");
		break;
	case DropBindingVerdict::Unknown:
		OBVR_LOG("Hands: WARNING - the game's Run binding could not be read from Oblivion.ini; the "
		         "inventory drop (left A) assumes Run on Left Shift");
		break;
	case DropBindingVerdict::RunNotOnShift:
		OBVR_LOG("Hands: WARNING - Oblivion.ini binds Run to scan code 0x%02X (%s), not Left Shift "
		         "(0x2A): the inventory drop (left A) needs vanilla's Shift and may do nothing. "
		         "Bind Run back to Left Shift in the game's controls.",
		         run.key, path);
		break;
	case DropBindingVerdict::RunKeyDiffers:
		OBVR_LOG("Hands: WARNING - OBVR.ini RunKey (virtual key 0x%02X, scan 0x%02X) is not the key the "
		         "game's Run is on (0x%02X, %s): the inventory drop (left A) and running press the wrong "
		         "key. Set [Hands] RunKey=0x10 (Shift).",
		         runKey, scan, run.key, path);
		break;
	}
}

}  // namespace obvr::game
