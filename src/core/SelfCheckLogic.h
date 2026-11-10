#pragma once

#include <cstdio>
#include <cstring>

#include "core/Types.h"

// The install self-check: what OBVR finds about its own installation at
// startup, and what it tells the player. Everything here decides over plain
// values - the facts the game side gathers (game/SelfCheck.cpp) go in, one
// verdict per check comes out, with the line for OBVR.log and the lines for
// the panel in the main menu. Each check and the evidence for how its fact is
// read is in docs/self-check-spec.md; every requirement is the README's.
namespace obvr::selfcheck {

// Pending: the fact is not there yet (no device before the main menu, no
// controller before SteamVR names it). Unverified: OBVR looked and could not
// tell - never a guess either way. A hint is a likely cause, not a fault.
enum class State : UInt8 { Pending, Ok, Problem, Hint, Unverified };

enum class Check : UInt8 {
	Oblivion,
	Xobse,
	Dxvk,
	DxvkConf,
	MenuQue,
	SteamOverlay,
	Files,
	OpenVr,
	Conflicts,
	Trifle,
	Controllers,
	Count
};
constexpr UInt32 kCheckCount = static_cast<UInt32>(Check::Count);

inline const char* CheckName(Check check) {
	switch (check) {
	case Check::Oblivion: return "oblivion";
	case Check::Xobse: return "xobse";
	case Check::Dxvk: return "dxvk";
	case Check::DxvkConf: return "dxvk.conf";
	case Check::MenuQue: return "menuque";
	case Check::SteamOverlay: return "steam-overlay";
	case Check::Files: return "files";
	case Check::OpenVr: return "openvr";
	case Check::Conflicts: return "conflicts";
	case Check::Trifle: return "trifle";
	case Check::Controllers: return "controllers";
	case Check::Count: break;
	}
	return "?";
}

inline const char* StateWord(State state) {
	switch (state) {
	case State::Pending: return "pending";
	case State::Ok: return "ok";
	case State::Problem: return "PROBLEM";
	case State::Hint: return "HINT";
	case State::Unverified: return "could not verify";
	}
	return "?";
}

// OBSE's code for Oblivion 1.2.0.416 (obse_common/obse_version.h, the value
// OBVR already refuses anything else with: game/GameAddresses.h).
constexpr UInt32 kOblivion12416 = 0x010201A0;
// The README: xOBSE 22.13 or newer.
constexpr UInt32 kMinimumObseMajor = 22;
constexpr UInt32 kMinimumObseMinor = 13;

enum class Device : UInt8 { NotYet, Native, Dxvk };
enum class MenuQue : UInt8 { NotYet, Missing, Unsupported, V16b };
enum class OpenVrDll : UInt8 { Missing, Not32Bit, Unreadable, Ok };
enum class Trifle : UInt8 { NotInstalled, ShadowOff, ShadowOn };
enum class Controller : UInt8 { Unknown, Index, Other };

// The files the OBVR zip ships that OBVR reads, by where they live: next to
// OBVR.dll (Data/OBSE/Plugins, "plugin") or under Data. The licences and the
// sounds' README are not read, OBVR.dll checks itself by running, and
// openvr_api.dll has a check of its own. tests/SelfCheckAssetsTest.py holds
// this list against what tools/package-release.py puts in the zip, and the
// doctor scripts' lists against this one.
struct ExpectedFile {
	bool plugin;
	const char* path;
};
constexpr ExpectedFile kExpectedFiles[] = {
	{true, "OBVR.ini"},
	{true, "OBVR_Controls.ini"},
	{true, "OBVR_Input\\actions.json"},
	{true, "OBVR_Input\\knuckles.json"},
	{true, "OBVR_Input\\oculus_touch.json"},
	{true, "OBVR_Sounds\\slap.wav"},
	{true, "OBVR_Stage\\loading_dome_dark.obj"},
	{true, "OBVR_Stage\\loading_dome_dark.mtl"},
	{true, "OBVR_Stage\\loading_dome_dark.png"},
	{false, "Menus\\generic\\OBVR_AdjustHands.xml"},
	{false, "Menus\\generic\\OBVR_Controls.xml"},
	{false, "Menus\\generic\\OBVR_Onboarding.xml"},
	{false, "Menus\\generic\\OBVR_SelfCheck.xml"},
	{false, "Menus\\generic\\OBVR_Settings.xml"},
	{false, "Menus\\generic\\OBVR_Update.xml"},
	{false, "Menus\\prefabs\\OBVR\\button_highlight.xml"},
	{false, "Textures\\Effects\\OBVR\\CutCloth.dds"},
	{false, "Textures\\Effects\\OBVR\\CutLeather.dds"},
	{false, "Textures\\Effects\\OBVR\\CutSkin.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_1_left.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_1_left_index.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_1_right.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_1_right_index.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_2_left.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_2_right.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_3_left.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_3_left_index.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_3_right.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_3_right_index.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_4_left.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_4_right.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_5_left.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_5_left_index.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_5_right.dds"},
	{false, "Textures\\Menus\\OBVR\\controls_5_right_index.dds"},
	{false, "Textures\\Menus\\OBVR\\tpose.dds"},
	{false, "Meshes\\characters\\_male\\specialanims\\HandToHandStagger_OBVRMirror.kf"},
	{false, "Meshes\\characters\\_male\\specialanims\\OneHandStagger_OBVRMirror.kf"},
	{false, "Meshes\\characters\\_male\\specialanims\\StaffStagger_OBVRMirror.kf"},
	{false, "Meshes\\characters\\_male\\specialanims\\TwoHandStagger_OBVRMirror.kf"},
};
constexpr UInt32 kExpectedFileCount = sizeof(kExpectedFiles) / sizeof(kExpectedFiles[0]);

// Everything the checks decide on. The game side fills it; a fact it could
// not read stays at its default, which every check treats as "not known".
struct Facts {
	// Under Wine/Proton (ntdll exports wine_get_version).
	bool proton = false;
	// OBSE's Query hands this over; 0 before it did.
	UInt32 oblivionVersion = 0;
	// obse.log's "OBSE: initialize (version = 22.13 010201A0)".
	bool obseLogRead = false;
	UInt32 obseMajor = 0;
	UInt32 obseMinor = 0;
	// The game's Direct3D device, once there is one: DXVK's or not.
	Device device = Device::NotYet;
	// The d3d9.dll the game loaded, read from its file: whether it carries
	// the "-gplasync" tag in its version string (Windows only).
	bool d3d9Read = false;
	bool d3d9Gplasync = false;
	// dxvk.conf (or the file DXVK_CONFIG_FILE names): found, and whether its
	// two README lines are there with the README's values.
	bool dxvkConfFound = false;
	bool dxvkConfFromEnvironment = false;
	bool memoryDefragOff = false;
	bool cachedWriteOnlyOn = false;
	MenuQue menuQue = MenuQue::NotYet;
	// GameOverlayRenderer.dll is loaded in the game.
	bool overlayLoaded = false;
	// The files of kExpectedFiles not found; the first one by name.
	UInt32 missingFiles = 0;
	const char* firstMissing = nullptr;
	OpenVrDll openVr = OpenVrDll::Missing;
	bool enhancedCamera = false;
	bool oblivionReloaded = false;
	Trifle trifle = Trifle::NotInstalled;
	// Full VR on, and what SteamVR says the hands and the headset are.
	bool fullVr = false;
	Controller right = Controller::Unknown;
	Controller left = Controller::Unknown;
	bool hmdRead = false;
	bool hmdQuestLike = false;
};

constexpr UInt32 kResultText = 200;
struct Result {
	Check check = Check::Oblivion;
	State state = State::Pending;
	char text[kResultText] = {};
};

namespace detail {
inline void Say(Result& r, State state, const char* text) {
	r.state = state;
	std::snprintf(r.text, sizeof(r.text), "%s", text);
}
inline char Lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
inline bool ContainsNoCase(const char* text, const char* word) {
	if (text == nullptr || word == nullptr || word[0] == '\0') {
		return false;
	}
	for (const char* at = text; *at != '\0'; ++at) {
		const char* a = at;
		const char* b = word;
		while (*a != '\0' && *b != '\0' && Lower(*a) == Lower(*b)) {
			++a;
			++b;
		}
		if (*b == '\0') {
			return true;
		}
	}
	return false;
}
}  // namespace detail

// "OBSE: initialize (version = 22.13 010201A0)" - xOBSE's own first line in
// obse.log (obse/obse/obse.cpp, _MESSAGE("OBSE: initialize (version = %d.%d
// %08X)"), read through deepwiki llde/xOBSE 2026-11-10; the line in the Linux
// install's obse.log the same day). False for a text without that line.
inline bool ParseObseLog(const char* text, UInt32 length, UInt32& major, UInt32& minor) {
	major = 0;
	minor = 0;
	if (text == nullptr) {
		return false;
	}
	static const char kKey[] = "OBSE: initialize (version = ";
	const UInt32 keyLength = sizeof(kKey) - 1;
	for (UInt32 at = 0; at + keyLength <= length; ++at) {
		if (std::memcmp(text + at, kKey, keyLength) != 0) {
			continue;
		}
		UInt32 p = at + keyLength;
		UInt32 a = 0;
		UInt32 b = 0;
		bool digitsA = false;
		bool digitsB = false;
		while (p < length && text[p] >= '0' && text[p] <= '9' && a < 100000u) {
			a = a * 10u + static_cast<UInt32>(text[p++] - '0');
			digitsA = true;
		}
		if (!digitsA || p >= length || text[p] != '.') {
			return false;
		}
		++p;
		while (p < length && text[p] >= '0' && text[p] <= '9' && b < 100000u) {
			b = b * 10u + static_cast<UInt32>(text[p++] - '0');
			digitsB = true;
		}
		if (!digitsB) {
			return false;
		}
		major = a;
		minor = b;
		return true;
	}
	return false;
}

// dxvk.conf's two README lines. DXVK reads "key = value" with any spaces
// round the '=', '#' starts a comment, and a boolean is "true" or "false" in
// any case (src/util/config/config.cpp, read through deepwiki
// doitsujin/dxvk 2026-11-10). The last line for a key wins, as it does there.
struct DxvkConfVerdict {
	bool memoryDefragOff = false;
	bool cachedWriteOnlyOn = false;
};
inline DxvkConfVerdict ParseDxvkConf(const char* text, UInt32 length) {
	DxvkConfVerdict v;
	if (text == nullptr) {
		return v;
	}
	UInt32 p = 0;
	while (p < length) {
		UInt32 end = p;
		while (end < length && text[end] != '\n') {
			++end;
		}
		// One line, without its comment and its surrounding space.
		UInt32 lineEnd = p;
		while (lineEnd < end && text[lineEnd] != '#') {
			++lineEnd;
		}
		auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
		UInt32 a = p;
		while (a < lineEnd && space(text[a])) {
			++a;
		}
		UInt32 eq = a;
		while (eq < lineEnd && text[eq] != '=') {
			++eq;
		}
		if (eq < lineEnd) {
			UInt32 keyEnd = eq;
			while (keyEnd > a && space(text[keyEnd - 1])) {
				--keyEnd;
			}
			UInt32 b = eq + 1;
			while (b < lineEnd && space(text[b])) {
				++b;
			}
			UInt32 valueEnd = lineEnd;
			while (valueEnd > b && space(text[valueEnd - 1])) {
				--valueEnd;
			}
			auto keyIs = [&](const char* key) {
				const UInt32 n = static_cast<UInt32>(std::strlen(key));
				return keyEnd - a == n && std::memcmp(text + a, key, n) == 0;
			};
			auto valueIs = [&](const char* word) {
				const UInt32 n = static_cast<UInt32>(std::strlen(word));
				if (valueEnd - b != n) {
					return false;
				}
				for (UInt32 i = 0; i < n; ++i) {
					if (detail::Lower(text[b + i]) != word[i]) {
						return false;
					}
				}
				return true;
			};
			if (keyIs("dxvk.enableMemoryDefrag")) {
				v.memoryDefragOff = valueIs("false");
			} else if (keyIs("d3d9.cachedWriteOnlyBuffers")) {
				v.cachedWriteOnlyOn = valueIs("true");
			}
		}
		p = end + 1;
	}
	return v;
}

// Trifle.ini's EnableFirstPersonShadow, the README's: 1 hides the player's
// body every frame. Read as the first "EnableFirstPersonShadow=<n>" line in
// any section; a ';' line is a comment.
inline Trifle ParseTrifleIni(const char* text, UInt32 length) {
	if (text == nullptr) {
		return Trifle::NotInstalled;
	}
	static const char kKey[] = "EnableFirstPersonShadow";
	const UInt32 keyLength = sizeof(kKey) - 1;
	UInt32 p = 0;
	while (p < length) {
		UInt32 end = p;
		while (end < length && text[end] != '\n') {
			++end;
		}
		UInt32 a = p;
		while (a < end && (text[a] == ' ' || text[a] == '\t')) {
			++a;
		}
		if (a + keyLength <= end && text[a] != ';' && std::memcmp(text + a, kKey, keyLength) == 0) {
			UInt32 b = a + keyLength;
			while (b < end && (text[b] == ' ' || text[b] == '\t')) {
				++b;
			}
			if (b < end && text[b] == '=') {
				++b;
				while (b < end && (text[b] == ' ' || text[b] == '\t')) {
					++b;
				}
				return b < end && text[b] == '1' ? Trifle::ShadowOn : Trifle::ShadowOff;
			}
		}
		p = end + 1;
	}
	return Trifle::ShadowOff;
}

// Whether the headset is a Meta one, from SteamVR's Prop_ModelNumber_String
// (1001) and Prop_ManufacturerName_String (1005) of the HMD (openvr.h, read
// through deepwiki ValveSoftware/openvr 2026-11-10). A heuristic: which
// strings Virtual Desktop, ALVR, Steam Link and Link report is not verified.
inline bool HmdLooksLikeQuest(const char* model, const char* manufacturer) {
	return detail::ContainsNoCase(model, "quest") || detail::ContainsNoCase(manufacturer, "oculus") ||
	       detail::ContainsNoCase(manufacturer, "meta");
}

// A PE file's machine (IMAGE_FILE_HEADER.Machine): 0x014C is 32-bit x86.
// From the file's first bytes; false when they are no PE header.
inline bool PeMachine(const UInt8* bytes, UInt32 length, UInt16& machine) {
	machine = 0;
	if (bytes == nullptr || length < 0x40 || bytes[0] != 'M' || bytes[1] != 'Z') {
		return false;
	}
	const UInt32 pe = static_cast<UInt32>(bytes[0x3C]) | (static_cast<UInt32>(bytes[0x3D]) << 8) |
	                  (static_cast<UInt32>(bytes[0x3E]) << 16) | (static_cast<UInt32>(bytes[0x3F]) << 24);
	if (pe > length || length - pe < 6 || bytes[pe] != 'P' || bytes[pe + 1] != 'E' || bytes[pe + 2] != 0 ||
	    bytes[pe + 3] != 0) {
		return false;
	}
	machine = static_cast<UInt16>(bytes[pe + 4] | (bytes[pe + 5] << 8));
	return true;
}
constexpr UInt16 kMachineI386 = 0x014C;

namespace detail {
// The game and its renderer: Oblivion, xOBSE, DXVK and dxvk.conf.
inline void EvaluateGame(const Facts& f, Result (&out)[kCheckCount]) {
	char line[kResultText];

	Result& oblivion = out[static_cast<UInt32>(Check::Oblivion)];
	if (f.oblivionVersion == 0) {
		Say(oblivion, State::Pending, "");
	} else if (f.oblivionVersion == kOblivion12416) {
		Say(oblivion, State::Ok, "Oblivion 1.2.0.416");
	} else {
		Say(oblivion, State::Problem, "Oblivion is not version 1.2.0.416 - install the last official patch");
	}

	Result& xobse = out[static_cast<UInt32>(Check::Xobse)];
	if (!f.obseLogRead) {
		Say(xobse, State::Unverified, "the xOBSE version is not in obse.log next to Oblivion.exe");
	} else if (f.obseMajor > kMinimumObseMajor ||
	           (f.obseMajor == kMinimumObseMajor && f.obseMinor >= kMinimumObseMinor)) {
		std::snprintf(line, sizeof(line), "xOBSE %u.%u", f.obseMajor, f.obseMinor);
		Say(xobse, State::Ok, line);
	} else {
		std::snprintf(line, sizeof(line), "xOBSE %u.%u is too old - install xOBSE 22.13 or newer", f.obseMajor,
		              f.obseMinor);
		Say(xobse, State::Problem, line);
	}

	Result& dxvk = out[static_cast<UInt32>(Check::Dxvk)];
	if (f.device == Device::NotYet) {
		Say(dxvk, State::Pending, "");
	} else if (f.device == Device::Native) {
		Say(dxvk, State::Problem,
		    f.proton ? "DXVK is not active - Proton brings DXVK unless it was switched off (PROTON_USE_WINED3D)"
		             : "DXVK is not active - copy d3d9.dll from the gplasync zip's x32 folder next to Oblivion.exe");
	} else if (f.proton) {
		Say(dxvk, State::Ok, "DXVK active (Proton's own)");
	} else if (!f.d3d9Read) {
		Say(dxvk, State::Unverified, "DXVK active, but which build could not be read from d3d9.dll");
	} else if (f.d3d9Gplasync) {
		Say(dxvk, State::Ok, "DXVK gplasync active");
	} else {
		Say(dxvk, State::Hint,
		    "DXVK is active, but not the gplasync build - the README's gplasync build stutters less");
	}

	Result& conf = out[static_cast<UInt32>(Check::DxvkConf)];
	const char* confName = f.dxvkConfFromEnvironment ? "the file DXVK_CONFIG_FILE names" : "dxvk.conf";
	if (!f.dxvkConfFound) {
		std::snprintf(line, sizeof(line), "%s is missing - make dxvk.conf next to Oblivion.exe with the README's "
		              "two lines", confName);
		Say(conf, State::Problem, line);
	} else if (f.memoryDefragOff && f.cachedWriteOnlyOn) {
		std::snprintf(line, sizeof(line), "%s has both lines", confName);
		Say(conf, State::Ok, line);
	} else {
		const char* lacking = !f.memoryDefragOff && !f.cachedWriteOnlyOn
		                          ? "dxvk.enableMemoryDefrag = False and d3d9.cachedWriteOnlyBuffers = True"
		                          : (!f.memoryDefragOff ? "dxvk.enableMemoryDefrag = False"
		                                                : "d3d9.cachedWriteOnlyBuffers = True");
		std::snprintf(line, sizeof(line), "%s lacks the line %s - add it", confName, lacking);
		Say(conf, State::Problem, line);
	}

}

// What sits beside it: MenuQue, the overlay, OBVR's files, the mods that
// clash, and the controllers.
inline void EvaluateAround(const Facts& f, Result (&out)[kCheckCount]) {
	char line[kResultText];
	Result& menuQue = out[static_cast<UInt32>(Check::MenuQue)];
	switch (f.menuQue) {
	case MenuQue::NotYet: Say(menuQue, State::Pending, ""); break;
	case MenuQue::Missing:
		Say(menuQue, State::Problem, "MenuQue is not installed - install MenuQue v16b for the menus in the headset");
		break;
	case MenuQue::Unsupported:
		Say(menuQue, State::Problem, "this MenuQue is not v16b - install MenuQue v16b");
		break;
	case MenuQue::V16b: Say(menuQue, State::Ok, "MenuQue v16b"); break;
	}

	Result& overlay = out[static_cast<UInt32>(Check::SteamOverlay)];
	if (f.overlayLoaded) {
		Say(overlay, State::Problem,
		    "the Steam overlay is on - Steam Library, right-click Oblivion > Properties > General, overlay off");
	} else if (f.proton) {
		Say(overlay, State::Unverified, "no overlay DLL in the game; under Proton the overlay may not show as one");
	} else {
		Say(overlay, State::Ok, "off");
	}

	Result& files = out[static_cast<UInt32>(Check::Files)];
	if (f.missingFiles == 0) {
		std::snprintf(line, sizeof(line), "all %u files", kExpectedFileCount);
		Say(files, State::Ok, line);
	} else {
		std::snprintf(line, sizeof(line), "%u OBVR file%s missing, e.g. %s - unpack the OBVR zip into Data again",
		              f.missingFiles, f.missingFiles == 1 ? " is" : "s are",
		              f.firstMissing != nullptr ? f.firstMissing : "?");
		Say(files, State::Problem, line);
	}

	Result& openVr = out[static_cast<UInt32>(Check::OpenVr)];
	switch (f.openVr) {
	case OpenVrDll::Missing:
		Say(openVr, State::Problem, "openvr_api.dll is missing next to OBVR.dll - unpack the OBVR zip again");
		break;
	case OpenVrDll::Not32Bit:
		Say(openVr, State::Problem, "openvr_api.dll is not the 32-bit one - take the one from the OBVR zip");
		break;
	case OpenVrDll::Unreadable:
		Say(openVr, State::Unverified, "openvr_api.dll could not be read");
		break;
	case OpenVrDll::Ok: Say(openVr, State::Ok, "openvr_api.dll, 32-bit"); break;
	}

	Result& conflicts = out[static_cast<UInt32>(Check::Conflicts)];
	if (f.enhancedCamera && f.oblivionReloaded) {
		Say(conflicts, State::Problem,
		    "Enhanced Camera and Oblivion Reloaded do not work with OBVR - remove OBSE_EnhancedCamera.dll and "
		    "OblivionReloaded.dll");
	} else if (f.enhancedCamera) {
		Say(conflicts, State::Problem, "Enhanced Camera does not work with OBVR - remove OBSE_EnhancedCamera.dll");
	} else if (f.oblivionReloaded) {
		Say(conflicts, State::Problem, "Oblivion Reloaded does not work with OBVR - remove OblivionReloaded.dll");
	} else {
		Say(conflicts, State::Ok, "no Enhanced Camera, no Oblivion Reloaded");
	}

	Result& trifle = out[static_cast<UInt32>(Check::Trifle)];
	switch (f.trifle) {
	case Trifle::NotInstalled: Say(trifle, State::Ok, "not installed"); break;
	case Trifle::ShadowOff: Say(trifle, State::Ok, "first-person shadow off"); break;
	case Trifle::ShadowOn:
		Say(trifle, State::Problem, "Trifle hides your body - set EnableFirstPersonShadow=0 in Trifle.ini");
		break;
	}

	Result& controllers = out[static_cast<UInt32>(Check::Controllers)];
	if (!f.fullVr) {
		Say(controllers, State::Ok, "not checked, Full VR is off");
	} else if (f.right == Controller::Unknown && f.left == Controller::Unknown) {
		Say(controllers, State::Pending, "");
	} else if ((f.right == Controller::Index || f.left == Controller::Index) && f.hmdRead && f.hmdQuestLike) {
		Say(controllers, State::Hint,
		    "a Quest headset with Index controllers: in Virtual Desktop or ALVR, turn off the Index controller "
		    "emulation");
	} else {
		Say(controllers, State::Ok,
		    f.right == Controller::Index || f.left == Controller::Index ? "Index controllers" : "controllers known");
	}
}
}  // namespace detail

// Every check over the facts, in Check's order.
inline void Evaluate(const Facts& f, Result (&out)[kCheckCount]) {
	for (UInt32 i = 0; i < kCheckCount; ++i) {
		out[i] = Result{};
		out[i].check = static_cast<Check>(i);
	}
	detail::EvaluateGame(f, out);
	detail::EvaluateAround(f, out);
}

// A check's log line: "Self-check: dxvk.conf PROBLEM - <what to do>".
inline void FormatLogLine(const Result& r, char* out, UInt32 capacity) {
	if (out == nullptr || capacity == 0) {
		return;
	}
	if (r.text[0] != '\0') {
		std::snprintf(out, capacity, "Self-check: %s %s - %s", CheckName(r.check), StateWord(r.state), r.text);
	} else {
		std::snprintf(out, capacity, "Self-check: %s %s", CheckName(r.check), StateWord(r.state));
	}
}

// A check is written to the log once it has an answer, and again only when
// the answer changes (a controller read late, a device made after the
// first look).
inline bool ShouldLog(State before, State now) { return now != State::Pending && now != before; }

// The panel waits for every check but the controllers' (those come when
// SteamVR names them, which may be long after the main menu).
inline bool PanelReady(const Result (&results)[kCheckCount]) {
	for (const Result& r : results) {
		if (r.check != Check::Controllers && r.state == State::Pending) {
			return false;
		}
	}
	return true;
}

inline bool PanelWanted(const Result (&results)[kCheckCount], bool showPanel) {
	if (!showPanel) {
		return false;
	}
	for (const Result& r : results) {
		if (r.state == State::Problem || r.state == State::Hint) {
			return true;
		}
	}
	return false;
}

// The panel's lines: the problems first, then the hints ("Hint: ..."), at
// most `maxLines`; when more are there than fit, the last line says how many
// more and where they are.
constexpr UInt32 kPanelLines = 5;
constexpr UInt32 kPanelLineChars = 200;
inline UInt32 PanelLines(const Result (&results)[kCheckCount], char (&lines)[kPanelLines][kPanelLineChars]) {
	UInt32 total = 0;
	for (const Result& r : results) {
		if (r.state == State::Problem || r.state == State::Hint) {
			++total;
		}
	}
	const UInt32 room = total > kPanelLines ? kPanelLines - 1 : total;
	UInt32 written = 0;
	for (int pass = 0; pass < 2; ++pass) {
		const State wanted = pass == 0 ? State::Problem : State::Hint;
		for (const Result& r : results) {
			if (r.state != wanted || written >= room) {
				continue;
			}
			std::snprintf(lines[written], kPanelLineChars, "%s%s", wanted == State::Hint ? "Hint: " : "", r.text);
			++written;
		}
	}
	if (total > room) {
		std::snprintf(lines[written], kPanelLineChars, "...and %u more - see OBVR.log next to Oblivion.exe",
		              total - room);
		++written;
	}
	for (UInt32 i = written; i < kPanelLines; ++i) {
		lines[i][0] = '\0';
	}
	return written;
}

}  // namespace obvr::selfcheck
