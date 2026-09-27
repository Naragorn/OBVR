#include "test/HandScriptRuntime.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "core/Config.h"
#include "core/Log.h"
#include "game/KeyScanCodes.h"
#include "game/MeleeHits.h"
#include "platform/PluginPath.h"
#include "platform/Win32Min.h"
#include "test/HandScript.h"
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

void StepHandScriptFrame(float dt, bool inWorld, bool menuUp, const vr::OpenVRBackend& backend) {
	g_markedThisFrame = false;
	LoadIfNamed();
	if (!g_loaded) {
		return;
	}
	if (!g_started) {
		if (!inWorld || menuUp) {
			return;
		}
		g_started = true;
		// The weapon's form ID, for a scenario's console lines ({weapon}).
		OBVR_LOG("HandScript: started - equipped weapon form %08X, Hands %d, teleport %d noise %d, "
		         "holsters %d, fists %d, quick menu %d",
		         game::EquippedWeaponFormId(), GetConfig().fullVrMode ? 1 : 0,
		         GetConfig().hands.teleport.enabled ? 1 : 0,
		         GetConfig().hands.teleport.makesNoise ? 1 : 0, GetConfig().hands.holster.enabled ? 1 : 0,
		         GetConfig().hands.fist.enabled ? 1 : 0, GetConfig().hands.quickMenu.enabled ? 1 : 0);
		dt = 0.0f;
	}
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
		}
		OBVR_LOG("HandScript: %s %s (t=%.2f s, line %u)",
		         said->op == ScriptOp::Mark ? "mark" : "log", said->text.c_str(),
		         static_cast<double>(g_clock), said->line);
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

const char* HandScriptMarkName() { return g_markName; }

void ScriptedHands(const vr::Quaternion& headOrientation, const NiPoint3& headPosition,
                   vr::HandPose& right, vr::HandPose& left) {
	right = ScriptedHandPose(g_run.pose.right, headOrientation, headPosition);
	left = ScriptedHandPose(g_run.pose.left, headOrientation, headPosition);
}

bool HandScriptHeadActive() { return HandScriptDrivesHands() && g_run.pose.head.synthetic; }

void GetHandScriptHeadPose(vr::Quaternion& orientation, NiPoint3& position) {
	orientation = ScriptHeadOrientation(g_run.pose.head);
	position = g_run.pose.head.position;
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
