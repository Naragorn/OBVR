#include "test/HandScriptRuntime.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "game/ConsoleLine.h"
#include "game/KeyScanCodes.h"
#include "game/MeleeHits.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "render/SceneRenderHook.h"
#include "test/HandScript.h"
#include "test/HandScriptConsole.h"
#include "test/HandScriptPose.h"
#include "vr/OpenVRBackend.h"

namespace obvr::test {
namespace {

HandScript g_script;
HandScriptRun g_run;
char g_loadedName[128] = "";
bool g_loaded = false;
bool g_started = false;
float g_clock = 0.0f;
bool g_markedThisFrame = false;
char g_markName[64] = "";
char g_action[64] = "";
// The runner's work done from inside (test/HandScriptConsole.h).
ScriptConsoleFeed g_consoleFeed;
ScriptSaveLoad g_saveLoad;
UInt32 g_weaponForm = 0;
UInt32 g_nearForm = 0;
UInt32 g_actorForm = 0;

// [Debug] HandScriptLoad: "load <save>" for the main menu's console, once
// it has stood a moment (StepScriptSaveLoad).
void StepSaveLoad(float dt, bool mainMenuUp) {
	const char* save = GetConfig().handScriptLoad;
	const bool haveSave = save[0] != '\0';
	// The script's console-menu lines first, four a frame, then the load.
	static size_t s_menuHanded = 0;
	const size_t menuTotal = g_loaded ? g_script.consoleMenu.size() : 0;
	const bool ask = StepScriptSaveLoad(g_saveLoad, haveSave, mainMenuUp, dt, s_menuHanded < menuTotal);
	const UInt32 due = ScriptMenuLinesDue(g_saveLoad, haveSave, mainMenuUp, s_menuHanded, menuTotal);
	for (UInt32 i = 0; i < due; ++i) {
		const std::string& menuLine = g_script.consoleMenu[s_menuHanded++];
		const bool menuTaken = game::RequestConsoleLine(menuLine.c_str());
		OBVR_LOG("HandScript: asked the main menu's console for \"%s\" before the load (%u of %u, %s)",
		         menuLine.c_str(), static_cast<UInt32>(s_menuHanded), static_cast<UInt32>(menuTotal),
		         menuTaken ? "taken" : "refused");
	}
	if (!ask) {
		return;
	}
	char line[96];
	std::snprintf(line, sizeof(line), "load %s", save);
	const bool taken = game::RequestConsoleLine(line);
	OBVR_LOG("HandScript: asked the main menu's console for \"%s\" (ask %u of %u, %s)", line, g_saveLoad.asked,
	         kScriptLoadAsksMost, taken ? "taken" : "refused");
}

// A plugin's form ID base by its name, for {form:<plugin>:<local>}
// (test/HandScriptConsole.h), as Oblivion Walkers finds its own (src/Rise.cpp
// FindEspBase, in the game 2026-10-28): OblivionESL's light plugins first
// (ESL_GetFormIDBase, 0xFFFFFFFF for none), then the DataHandler's loaded
// mods by their runtime index (xOBSE GameData.h: the DataHandler at
// [0x00B33A98], the count at +0x8D0, the entries by index at +0x8D4, each
// file name at +0x1C), the name compared without case.
bool ResolveScriptPlugin(const std::string& plugin, UInt32& base, bool& light) {
	const HMODULE esl = GetModuleHandleA("OblivionESL");
	if (esl != nullptr) {
		using BaseFn = UInt32(__cdecl*)(const char* modName);
		const auto eslBase = reinterpret_cast<BaseFn>(GetProcAddress(esl, "ESL_GetFormIDBase"));
		const UInt32 found = eslBase != nullptr ? eslBase(plugin.c_str()) : 0xFFFFFFFFu;
		if (found != 0xFFFFFFFFu) {
			base = found;
			light = true;
			return true;
		}
	}
	constexpr UInt32 kDataHandlerPointer = 0x00B33A98;
	constexpr UInt32 kModCountOffset = 0x8D0;
	constexpr UInt32 kModsByIndexOffset = 0x8D4;
	constexpr UInt32 kModNameOffset = 0x1C;
	const UInt32 handler = *reinterpret_cast<const UInt32*>(kDataHandlerPointer);
	if (!mem::LooksLikeObjectAddress(handler)) {
		return false;
	}
	const UInt32 count = *reinterpret_cast<const UInt32*>(handler + kModCountOffset);
	for (UInt32 i = 0; i < count && i < 0xFF; ++i) {
		const UInt32 entry = *reinterpret_cast<const UInt32*>(handler + kModsByIndexOffset + i * 4);
		if (mem::LooksLikeObjectAddress(entry) &&
		    _stricmp(reinterpret_cast<const char*>(entry + kModNameOffset), plugin.c_str()) == 0) {
			base = i << 24;
			light = false;
			return true;
		}
	}
	return false;
}

// [Debug] HandScriptConsole: one queued console line a frame to the game.
void StepConsoleFeed(float dt) {
	if (!GetConfig().handScriptConsole) {
		return;
	}
	static const ScriptFormResolver resolve = &ResolveScriptPlugin;
	bool unresolved = false;
	const ScriptConsoleStep step =
		StepScriptConsoleFeed(g_consoleFeed, dt, g_weaponForm, g_nearForm, g_actorForm,
		                      [](const std::string& line) { return game::RequestConsoleLine(line.c_str()); },
		                      &resolve, &unresolved);
	if (unresolved && (step.handed || step.dropped)) {
		OBVR_LOG("HandScript: WARNING - a {form:<plugin>:<local hex>} in \"%s\" is not resolved - the plugin is "
		         "not loaded, or the token is malformed",
		         step.line.c_str());
	}
	if (step.handed) {
		OBVR_LOG("HandScript: console \"%s\" handed to the game (t=%.2f s)", step.line.c_str(),
		         static_cast<double>(g_clock));
	} else if (step.dropped) {
		OBVR_LOG("HandScript: console \"%s\" given up - %s", step.line.c_str(),
		         step.line.size() > kScriptConsoleLineMost ? "longer than the console takes"
		                                                  : "the console refused it frame after frame");
	}
}

bool ReadWholeFile(const char* path, std::string& out) {
	FILE* file = nullptr;
	if (fopen_s(&file, path, "rb") != 0 || file == nullptr) {
		return false;
	}
	char buffer[4096];
	size_t got = 0;
	while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
		out.append(buffer, got);
	}
	std::fclose(file);
	return true;
}

// Loads the script the configuration names, once per name: a run keeps the
// script it started with even while the INI is reloaded under it.
void LoadIfNamed() {
	const char* name = GetConfig().handScript;
	if (name[0] == '\0' || std::strcmp(name, g_loadedName) == 0) {
		return;
	}
	strncpy_s(g_loadedName, name, _TRUNCATE);
	g_loaded = false;
	g_started = false;
	g_run = HandScriptRun{};
	g_clock = 0.0f;
	g_consoleFeed = ScriptConsoleFeed{};
	g_saveLoad = ScriptSaveLoad{};
	g_weaponForm = 0;
	g_nearForm = 0;
	g_actorForm = 0;
	char path[512];
	std::string text;
	if (!platform::BuildPluginPath(name, path, sizeof(path)) || !ReadWholeFile(path, text)) {
		OBVR_LOG("HandScript: WARNING - cannot read %s", name);
		return;
	}
	ScriptParseError error;
	if (!ParseHandScript(text, g_script, error)) {
		OBVR_LOG("HandScript: WARNING - %s line %u: %s; nothing of it runs", name, error.line,
		         error.message.c_str());
		return;
	}
	g_loaded = true;
	OBVR_LOG("HandScript: loaded %s, %u steps; it starts once the player is in the world", path,
	         static_cast<UInt32>(g_script.steps.size()));
}

}  // namespace

void NoteHandScriptNearestForm(UInt32 formId) { g_nearForm = formId; }
void NoteHandScriptNearestActor(UInt32 formId) { g_actorForm = formId; }

void StepHandScriptFrame(float dt, bool inWorld, bool menuUp, bool mainMenuUp, const vr::OpenVRBackend& backend) {
	g_markedThisFrame = false;
	LoadIfNamed();
	if (!g_loaded) {
		return;
	}
	if (!g_started) {
		// "start main-menu": no save asked for, the script starts on the main
		// menu itself (the laser on its buttons); otherwise once the player is
		// in the world with no menu up.
		if (g_script.startOnMainMenu) {
			if (!mainMenuUp || inWorld) {
				return;
			}
		} else {
			StepSaveLoad(dt, mainMenuUp && !inWorld);
			if (!inWorld || menuUp) {
				return;
			}
		}
		g_started = true;
		g_weaponForm = game::EquippedWeaponFormId();
		// The weapon's form ID, for a scenario's console lines ({weapon}).
		OBVR_LOG("HandScript: started - equipped weapon form %08X, Hands %d, teleport %d noise %d, "
		         "holsters %d, fists %d, quick menu %d",
		         g_weaponForm, GetConfig().fullVrMode ? 1 : 0,
		         GetConfig().hands.teleport.enabled ? 1 : 0,
		         GetConfig().hands.teleport.makesNoise ? 1 : 0, GetConfig().hands.holster.enabled ? 1 : 0,
		         GetConfig().hands.fist.enabled ? 1 : 0, GetConfig().hands.quickMenu.enabled ? 1 : 0);
		if (GetConfig().handScriptConsole) {
			QueueStartConsoleLines(g_script, g_consoleFeed);
			OBVR_LOG("HandScript: OBVR runs the script's console lines itself (HandScriptConsole) - %u at the start, "
			         "%u at marks",
			         static_cast<UInt32>(g_script.console.size()), static_cast<UInt32>(g_script.consoleAt.size()));
		}
		dt = 0.0f;
	}
	// Ahead of the finished check: a last mark's lines still go out.
	StepConsoleFeed(dt);
	if (g_run.finished) {
		return;
	}
	g_clock += dt;
	HandScriptEvents events;
	StepHandScript(g_script, g_run, dt, events);
	for (const ScriptStep* said : events.said) {
		if (said->op == ScriptOp::Mark) {
			g_markedThisFrame = true;
			strncpy_s(g_markName, said->text.c_str(), _TRUNCATE);
			if (GetConfig().handScriptConsole) {
				QueueConsoleLinesAt(g_script, said->text, g_consoleFeed);
			}
		}
		// The tick is the clock a profiler capture shares with the log (its
		// frames' start_tick, the same counter): tools/perf-phases.py cuts
		// a capture by the marks with it, in any stereo mode. The scene call
		// stays for reading the mark against the "Dual ... at scene call N"
		// lines; under aer it is 0 (the scene render hook is not installed).
		OBVR_LOG("HandScript: %s %s (t=%.2f s, line %u, scene call %u, tick %lld)",
		         said->op == ScriptOp::Mark ? "mark" : "log", said->text.c_str(),
		         static_cast<double>(g_clock), said->line, render::CurrentSceneCall(),
		         ReadPerformanceCounter());
	}
	for (const ScriptStep* key : events.keys) {
		const UInt32 code = static_cast<UInt32>(key->values[0]);
		const bool down = key->values[1] != 0.0f;
		UInt32 scan = game::UsScanCode(code);
		if (scan == 0) {
			scan = MapVirtualKeyA(code, MAPVK_VK_TO_VSC);
		}
		keybd_event(static_cast<UInt8>(code), static_cast<UInt8>(scan),
		            KEYEVENTF_SCANCODE | (game::IsExtendedKey(code) ? KEYEVENTF_EXTENDEDKEY : 0) |
		                (down ? 0 : KEYEVENTF_KEYUP),
		            0);
		OBVR_LOG("HandScript: key %02X %s (t=%.2f s)", code, down ? "down" : "up",
		         static_cast<double>(g_clock));
	}
	for (const ScriptStep* action : events.actions) {
		strncpy_s(g_action, action->text.c_str(), _TRUNCATE);
		OBVR_LOG("HandScript: action %s asked for (t=%.2f s)", g_action, static_cast<double>(g_clock));
	}
	if (events.mirror) {
		backend.ShowMirrorWindow();
		OBVR_LOG("HandScript: the headset view window was asked for");
	}
	if (events.dump) {
		backend.DumpCompositorImages();
		OBVR_LOG("HandScript: SteamVR was asked to dump the frame (t=%.2f s)",
		         static_cast<double>(g_clock));
	}
	if (events.finished) {
		OBVR_LOG("HandScript: finished (t=%.2f s)", static_cast<double>(g_clock));
	}
}

bool HandScriptDrivesHands() { return g_loaded && g_started; }

bool HandScriptMarkedThisFrame() { return g_markedThisFrame; }

bool HandScriptHeadFacesFighter() { return HandScriptDrivesHands() && g_run.pose.head.synthetic && g_run.pose.head.faceFighter; }

void TurnHandScriptHead(float degreesLeft) {
	float yaw = g_run.pose.head.yaw + degreesLeft;
	while (yaw > 180.0f) {
		yaw -= 360.0f;
	}
	while (yaw < -180.0f) {
		yaw += 360.0f;
	}
	g_run.pose.head.yaw = yaw;
}

const char* HandScriptMarkName() { return g_markName; }

void ScriptedHands(const vr::Quaternion& headOrientation, const NiPoint3& headPosition,
                   vr::HandPose& right, vr::HandPose& left) {
	// The hands hang from the body, not from the eyes: with the script's
	// own head they are placed from its level eyes (`head pos`), which do
	// not swing with a nod (ScriptHeadPosition); with the real headset
	// driving the head, from where it is.
	const NiPoint3 anchor = g_run.pose.head.synthetic ? g_run.pose.head.position : headPosition;
	right = ScriptedHandPose(g_run.pose.right, headOrientation, anchor);
	left = ScriptedHandPose(g_run.pose.left, headOrientation, anchor);
	// The velocity SteamVR would report, from where the scripted hand was a
	// frame ago - what a throw takes its speed from (a `pos` line moves the
	// hand in one frame: a throw at the speed of that jump).
	// The turning speed likewise, from the orientation a frame ago (a `rot`
	// line turns the hand in one frame: a flick at the speed of that turn).
	static NiPoint3 s_lastRight{0.0f, 0.0f, 0.0f};
	static NiPoint3 s_lastLeft{0.0f, 0.0f, 0.0f};
	static vr::Quaternion s_lastRightTurn = vr::Quaternion::Identity();
	static vr::Quaternion s_lastLeftTurn = vr::Quaternion::Identity();
	static float s_lastClock = -1.0f;
	const float dt = s_lastClock >= 0.0f ? g_clock - s_lastClock : 0.0f;
	if (dt > 0.0f) {
		right.velocity = (right.position - s_lastRight) * (1.0f / dt);
		left.velocity = (left.position - s_lastLeft) * (1.0f / dt);
		right.angularVelocity = vr::AngularVelocityBetween(s_lastRightTurn, right.orientation, dt);
		left.angularVelocity = vr::AngularVelocityBetween(s_lastLeftTurn, left.orientation, dt);
	}
	s_lastRight = right.position;
	s_lastLeft = left.position;
	s_lastRightTurn = right.orientation;
	s_lastLeftTurn = left.orientation;
	s_lastClock = g_clock;
}

// From the script's load on, not only from its start: before the start the
// headset's own pose anchored the world - the flat picture, the head's
// height - and with a headset awake on the desk (2026-10-24, its pose at
// -1.65 m) the scripted hands then stood two metres above the floor the
// sword lay on; asleep, as on every day before, the headset gave no pose and
// nothing was anchored to it.
bool HandScriptHeadActive() { return g_loaded && g_run.pose.head.synthetic; }

void GetHandScriptHeadPose(vr::Quaternion& orientation, NiPoint3& position) {
	orientation = ScriptHeadOrientation(g_run.pose.head);
	position = ScriptHeadPosition(g_run.pose.head);
}

void GetHandScriptHeadPoseMatrix(vr::openvr::HmdMatrix34& matrix) {
	vr::Quaternion orientation{};
	NiPoint3 position{};
	GetHandScriptHeadPose(orientation, position);
	const NiMatrix33 rotation = vr::ToMatrix(orientation);
	for (unsigned row = 0; row < 3; ++row) {
		for (unsigned column = 0; column < 3; ++column) {
			matrix.m[row][column] = rotation.data[row][column];
		}
	}
	matrix.m[0][3] = position.x;
	matrix.m[1][3] = position.y;
	matrix.m[2][3] = position.z;
}

bool TakeHandScriptAction(const char* name) {
	if (g_action[0] == '\0' || std::strcmp(g_action, name) != 0) {
		return false;
	}
	g_action[0] = '\0';
	return true;
}

}  // namespace obvr::test
