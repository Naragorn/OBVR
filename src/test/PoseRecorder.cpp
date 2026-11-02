#include "test/PoseRecorder.h"

#include <atomic>
#include <cstdio>
#include <ctime>
#include <string>

#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/PlayerAim.h"
#include "platform/PluginPath.h"
#include "test/HandScriptRuntime.h"
#include "test/PoseRecording.h"

namespace obvr::test {
namespace {

PoseRing& Ring() {
	static PoseRing s_ring;
	return s_ring;
}

std::atomic<bool>& SaveAsked() {
	static std::atomic<bool> s_asked{false};
	return s_asked;
}

// Where the player stood when the recording was saved, for the header: the
// position, the heading (the reference's rotZ, radians, at +0x28 - xOBSE
// GameObjects.h: rotX 0x20, rotY 0x24, rotZ 0x28, pos 0x2C) and the cell's
// form ID (parentCell at +0x40, a form's ID at +0x0C).
std::string PlaceComment() {
	char text[256] = "# the player's place not read\n";
	const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	NiPoint3 at{};
	if (mem::LooksLikeObjectAddress(player) && game::PlayerWorldPosition(at)) {
		const float heading = *reinterpret_cast<const float*>(player + 0x28);
		const UInt32 cell = *reinterpret_cast<const UInt32*>(player + addr::kRefParentCellOffset);
		const UInt32 cellId =
			mem::LooksLikeObjectAddress(cell) ? *reinterpret_cast<const UInt32*>(cell + addr::kFormIdOffset) : 0;
		std::snprintf(text, sizeof(text),
		              "# the player at the save: %.0f %.0f %.0f, heading %.1f degrees, in cell %08X\n"
		              "# (to start the replay there, add console-at lines such as player.setpos x %.0f)\n",
		              static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
		              static_cast<double>(heading * 57.2957795f), cellId, static_cast<double>(at.x));
	}
	return text;
}

void Save() {
	const PoseRing& ring = Ring();
	if (ring.samples.empty()) {
		OBVR_LOG("Pose recording: asked to save, but nothing is recorded yet (in the world, no menu up)");
		return;
	}
	const std::time_t now = std::time(nullptr);
	std::tm local{};
	localtime_s(&local, &now);
	char name[64];
	std::strftime(name, sizeof(name), "OBVR-Recording-%Y%m%d-%H%M%S.txt", &local);
	char path[512];
	if (!platform::BuildPluginPath(name, path, sizeof(path))) {
		OBVR_LOG("Pose recording: no path for %s - not saved", name);
		return;
	}
	char header[512];
	std::snprintf(header, sizeof(header),
	              "# A pose recording (test/PoseRecording.h): the headset and both controllers as SteamVR gave them,\n"
	              "# %u frames over %.1f s, saved %s.\n"
	              "# Play it: copy it to tools/hand-scripts/ and run\n"
	              "#   powershell -File tools\\hand-script-run.ps1 -Script tools\\hand-scripts\\<this file> -Direct\n"
	              "# with the expects of what is to be seen added.\n",
	              static_cast<UInt32>(ring.samples.size()), static_cast<double>(ring.seconds), name);
	const std::string text = PoseRecordingScript(ring, std::string(header) + PlaceComment());
	FILE* file = nullptr;
	if (fopen_s(&file, path, "wb") != 0 || file == nullptr) {
		OBVR_LOG("Pose recording: cannot write %s", path);
		return;
	}
	const size_t written = std::fwrite(text.data(), 1, text.size(), file);
	std::fclose(file);
	OBVR_LOG("Pose recording: saved %s - %u frames, %.1f s, %u bytes%s", path, static_cast<UInt32>(ring.samples.size()),
	         static_cast<double>(ring.seconds), static_cast<UInt32>(written),
	         written == text.size() ? "" : " - SHORT WRITE");
}

}  // namespace

void RequestPoseRecordingSave() { SaveAsked().store(true); }

void RecordPoseFrame(float dt, bool live, bool headValid, const vr::Quaternion& head, const NiPoint3& headPosition,
                     const vr::HandPose& right, const vr::HandPose& left) {
	const float keep = GetConfig().poseRecordSeconds;
	if (live && keep > 0.0f && dt > 0.0f && dt < 1.0f) {
		PoseSample s;
		s.dt = dt;
		s.headValid = headValid;
		s.head = head;
		s.headPosition = headPosition;
		s.right = right;
		s.left = left;
		PushPoseSample(Ring(), s, keep);
	}
	if (SaveAsked().exchange(false) | TakeHandScriptAction("pose_save")) {
		Save();
	}
}

}  // namespace obvr::test

namespace obvr::test {

void ScriptOrRecordHands(float dt, bool live, bool headValid, const vr::Quaternion& head, const NiPoint3& headPosition,
                         vr::HandPose& right, vr::HandPose& left) {
	if (HandScriptDrivesHands()) {
		ScriptedHands(headValid ? head : vr::Quaternion::Identity(),
		              headValid ? headPosition : NiPoint3{0.0f, 1.6f, 0.0f}, right, left);
	}
	RecordPoseFrame(dt, live, headValid, head, headPosition, right, left);
}

}  // namespace obvr::test
