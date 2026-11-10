#include "game/SelfCheck.h"

#include <cstring>

#include "core/Config.h"
#include "core/Log.h"
#include "core/SelfCheckNotes.h"
#include "game/MenuQueSignature.h"
#include "obse/NativeMenuApi.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "render/GameDevice.h"

namespace obvr::game {
namespace {

using namespace selfcheck;

// The check's state, one owner: written by OBSE's Query and Load, then by
// the task on the game's loop, read by the native menus on the same loop.
struct SelfCheckState {
	UInt32 oblivionVersion = 0;
	bool installed = false;
	bool gathered = false;
	bool evaluated = false;
	Facts facts;
	Result results[kCheckCount];
	State logged[kCheckCount] = {};
};

SelfCheckState& Self() {
	static SelfCheckState state;
	return state;
}

// Up to `capacity` bytes of a file, NUL-terminated; the length read, or -1
// when the file cannot be opened.
int ReadFilePrefix(const char* path, char* out, UInt32 capacity) {
	if (path == nullptr || out == nullptr || capacity < 2) {
		return -1;
	}
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | 0x2u /* FILE_SHARE_WRITE: the log may be open */, nullptr, OPEN_EXISTING, 0, nullptr);
	if (file == InvalidHandle()) {
		return -1;
	}
	DWORD read = 0;
	const BOOL ok = ReadFile(file, out, capacity - 1, &read, nullptr);
	CloseHandle(file);
	if (!ok) {
		return -1;
	}
	out[read] = '\0';
	return static_cast<int>(read);
}

// Whether a file holds `needle`, read in pieces that overlap by its length
// so it is found across a seam. -1 when the file cannot be read.
int FileContains(const char* path, const char* needle) {
	HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
	if (file == InvalidHandle()) {
		return -1;
	}
	static char buffer[64 * 1024];
	const UInt32 n = static_cast<UInt32>(std::strlen(needle));
	UInt32 kept = 0;
	int found = 0;
	for (;;) {
		DWORD read = 0;
		if (!ReadFile(file, buffer + kept, sizeof(buffer) - kept, &read, nullptr) || read == 0) {
			break;
		}
		const UInt32 have = kept + read;
		for (UInt32 i = 0; i + n <= have; ++i) {
			if (buffer[i] == needle[0] && std::memcmp(buffer + i, needle, n) == 0) {
				found = 1;
				break;
			}
		}
		if (found != 0) {
			break;
		}
		kept = have >= n ? n - 1 : have;
		std::memmove(buffer, buffer + have - kept, kept);
	}
	CloseHandle(file);
	return found;
}

bool FileThere(const char* path) { return path != nullptr && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }

// The facts that do not change while the game runs, read once.
void GatherOnce(Facts& f) {
	f.oblivionVersion = Self().oblivionVersion;
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	f.proton = ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;

	char path[512];
	static char text[64 * 1024];
	if (platform::BuildGamePath("obse.log", path, sizeof(path))) {
		const int length = ReadFilePrefix(path, text, 4096);
		f.obseLogRead = length > 0 && ParseObseLog(text, static_cast<UInt32>(length), f.obseMajor, f.obseMinor);
	}

	// DXVK reads DXVK_CONFIG_FILE when it is set, else dxvk.conf in the
	// working directory - the game folder (deepwiki doitsujin/dxvk).
	char confPath[512] = {};
	const DWORD fromEnv = GetEnvironmentVariableA("DXVK_CONFIG_FILE", confPath, sizeof(confPath));
	f.dxvkConfFromEnvironment = fromEnv > 0 && fromEnv < sizeof(confPath);
	if (f.dxvkConfFromEnvironment || platform::BuildGamePath("dxvk.conf", confPath, sizeof(confPath))) {
		const int length = ReadFilePrefix(confPath, text, sizeof(text));
		f.dxvkConfFound = length >= 0;
		if (length > 0) {
			const DxvkConfVerdict v = ParseDxvkConf(text, static_cast<UInt32>(length));
			f.memoryDefragOff = v.memoryDefragOff;
			f.cachedWriteOnlyOn = v.cachedWriteOnlyOn;
		}
	}

	// The d3d9.dll the game loaded, from its own file: the gplasync build
	// carries "-gplasync" in its version string ("v3.1.1-1-gplasync",
	// measured in the Windows install's d3d9.dll 2026-11-10). Not under
	// Proton, whose DXVK is its own.
	if (!f.proton) {
		HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
		char d3d9Path[512] = {};
		if (d3d9 != nullptr && GetModuleFileNameA(d3d9, d3d9Path, sizeof(d3d9Path)) > 0) {
			const int found = FileContains(d3d9Path, "-gplasync");
			f.d3d9Read = found >= 0;
			f.d3d9Gplasync = found == 1;
		}
	}

	HMODULE menuQue = GetModuleHandleA("Submodule.Game.dll");
	f.menuQue = menuQue == nullptr ? MenuQue::Missing
	            : MenuQueModuleIsV16b(reinterpret_cast<const UInt8*>(menuQue)) ? MenuQue::V16b
	                                                                           : MenuQue::Unsupported;
	f.overlayLoaded = GetModuleHandleA("GameOverlayRenderer.dll") != nullptr;
	f.enhancedCamera = GetModuleHandleA("OBSE_EnhancedCamera.dll") != nullptr;
	f.oblivionReloaded = GetModuleHandleA("OblivionReloaded.dll") != nullptr;

	f.missingFiles = 0;
	f.firstMissing = nullptr;
	for (const ExpectedFile& e : kExpectedFiles) {
		bool built = false;
		if (e.plugin) {
			built = platform::BuildPluginPath(e.path, path, sizeof(path));
		} else {
			char relative[256];
			std::snprintf(relative, sizeof(relative), "Data\\%s", e.path);
			built = platform::BuildGamePath(relative, path, sizeof(path));
		}
		if (!built || !FileThere(path)) {
			if (f.missingFiles++ == 0) {
				f.firstMissing = e.path;
			}
		}
	}

	if (GetConfig().forceSelfCheckProblem) {
		if (f.missingFiles++ == 0) {
			f.firstMissing = "none really - [Debug] ForceSelfCheckProblem=1";
		}
	}

	f.openVr = OpenVrDll::Missing;
	if (platform::BuildPluginPath("openvr_api.dll", path, sizeof(path))) {
		const int length = ReadFilePrefix(path, text, 4096);
		UInt16 machine = 0;
		f.openVr = length < 0                                                                  ? OpenVrDll::Missing
		           : !PeMachine(reinterpret_cast<const UInt8*>(text), static_cast<UInt32>(length), machine) ? OpenVrDll::Unreadable
		           : machine == kMachineI386                                                    ? OpenVrDll::Ok
		                                                                                        : OpenVrDll::Not32Bit;
	}

	f.trifle = Trifle::NotInstalled;
	if (platform::BuildPluginPath("Trifle.ini", path, sizeof(path))) {
		const int length = ReadFilePrefix(path, text, sizeof(text));
		if (length >= 0) {
			f.trifle = ParseTrifleIni(text, static_cast<UInt32>(length));
		}
	}
}

// The facts that come late: the device, the controllers, Full VR.
bool GatherLate(Facts& f) {
	bool changed = false;
	if (f.device == Device::NotYet) {
		const render::DeviceKind kind = render::IdentifyDevice(render::GetGameDevice());
		if (kind == render::DeviceKind::Dxvk || kind == render::DeviceKind::Native) {
			f.device = kind == render::DeviceKind::Dxvk ? Device::Dxvk : Device::Native;
			changed = true;
		}
	}
	const bool fullVr = GetConfig().fullVrMode;
	const SteamVrNotes notes = ReadSteamVrNotes();
	const Controller right = notes.right;
	const Controller left = notes.left;
	const bool hmdRead = notes.hmdRead;
	const bool questLike = notes.hmdQuestLike;
	if (fullVr != f.fullVr || right != f.right || left != f.left || hmdRead != f.hmdRead || questLike != f.hmdQuestLike) {
		f.fullVr = fullVr;
		f.right = right;
		f.left = left;
		f.hmdRead = hmdRead;
		f.hmdQuestLike = questLike;
		changed = true;
	}
	return changed;
}

// The xOBSE task: every frame, on the game's loop.
void Tick() {
	SelfCheckState& self = Self();
	bool changed = false;
	if (!self.gathered) {
		self.gathered = true;
		GatherOnce(self.facts);
		changed = true;
	}
	changed = GatherLate(self.facts) || changed;
	if (!changed) {
		return;
	}
	Evaluate(self.facts, self.results);
	self.evaluated = true;
	for (UInt32 i = 0; i < kCheckCount; ++i) {
		if (ShouldLog(self.logged[i], self.results[i].state)) {
			char line[320];
			FormatLogLine(self.results[i], line, sizeof(line));
			OBVR_LOG("%s", line);
			self.logged[i] = self.results[i].state;
		}
	}
}

}  // namespace

void NoteSelfCheckObse(UInt32 oblivionVersion) { Self().oblivionVersion = oblivionVersion; }

void InstallSelfCheck(const obse::Interface* api) {
	if (api == nullptr || api->obseVersion < 22 || api->QueryInterface == nullptr) {
		OBVR_LOG("Self-check: no xOBSE 22 interfaces - the install is not checked");
		return;
	}
	const auto tasks = static_cast<obse::TasksApi*>(api->QueryInterface(8));
	if (tasks == nullptr || tasks->EnqueueTask == nullptr || tasks->EnqueueTask(&Tick) == nullptr) {
		OBVR_LOG("Self-check: the task interface is missing - the install is not checked");
		return;
	}
	Self().installed = true;
}

bool SelfCheckPanel(char (&lines)[kPanelLines][kPanelLineChars], UInt32& count) {
	count = 0;
	const SelfCheckState& self = Self();
	if (!self.installed || !self.evaluated || !PanelReady(self.results) ||
	    !PanelWanted(self.results, GetConfig().selfCheckPanel)) {
		return false;
	}
	count = PanelLines(self.results, lines);
	return count > 0;
}

}  // namespace obvr::game
