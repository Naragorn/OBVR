#pragma once

// The pose recording (Nadi, 2026-11-02: "bau" - a harness that plays what a
// player really did): while the game runs OBVR keeps the last seconds of the
// headset and both controllers as SteamVR gave them - the poses in tracking
// space, SteamVR's own velocities, the buttons, the axes, the grip, the
// fingers - and on asking writes them out as a hand script
// (test/HandScript.h), one `frame` line per frame. The hand script plays
// such a line as it came: the head and the hands exactly where they were, no
// placing from the head, no speed taken from a jump. A bug met in the
// headset is then a scenario: the saved file, run by
// tools/hand-script-run.ps1, with the expects of the bug added.
//
// Pure: the ring of the last seconds, a frame's line and its reading back.
// Covered by pose_recording_test.

#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

#include "core/Types.h"
#include "game/NiMath.h"
#include "vr/HandInput.h"
#include "vr/Quaternion.h"

namespace obvr::test {

struct PoseSample {
	float dt = 0.0f;  // seconds since the frame before
	bool headValid = false;
	vr::Quaternion head = vr::Quaternion::Identity();
	NiPoint3 headPosition{0.0f, 0.0f, 0.0f};
	vr::HandPose right;
	vr::HandPose left;
};

// The last `seconds` of frames, the oldest dropped as new ones come.
struct PoseRing {
	std::deque<PoseSample> samples;  // in order, oldest first
	float seconds = 0.0f;             // the frames' dt summed
};

inline void PushPoseSample(PoseRing& ring, const PoseSample& sample, float keepSeconds) {
	if (!(keepSeconds > 0.0f) || !(sample.dt >= 0.0f)) {
		return;
	}
	ring.samples.push_back(sample);
	ring.seconds += sample.dt;
	while (ring.samples.size() > 1 && ring.seconds - ring.samples.front().dt >= keepSeconds) {
		ring.seconds -= ring.samples.front().dt;
		ring.samples.pop_front();
	}
}

namespace detail {

inline void AppendNumber(std::string& out, float v) {
	char text[32];
	std::snprintf(text, sizeof(text), " %.6g", static_cast<double>(v == v ? v : 0.0f));
	out += text;
}

inline void AppendHand(std::string& out, const vr::HandPose& h) {
	out += h.valid ? " 1" : " 0";
	out += h.kind == vr::ControllerKind::Index ? " i" : (h.kind == vr::ControllerKind::Other ? " o" : " u");
	const float numbers[] = {h.orientation.x, h.orientation.y, h.orientation.z, h.orientation.w,
	                         h.position.x,    h.position.y,    h.position.z,    h.velocity.x,
	                         h.velocity.y,    h.velocity.z,    h.angularVelocity.x, h.angularVelocity.y,
	                         h.angularVelocity.z, h.trigger,   h.thumbX,        h.thumbY,
	                         h.gripPull,      h.gripForce};
	for (float v : numbers) {
		AppendNumber(out, v);
	}
	char bits[24];
	std::snprintf(bits, sizeof(bits), " %llx", static_cast<unsigned long long>(h.buttonsPressed));
	out += bits;
	out += h.curlValid ? " 1" : " 0";
	for (float c : h.curl) {
		AppendNumber(out, c);
	}
	out += h.thumbValid ? " 1" : " 0";
	for (float t : h.thumb) {
		AppendNumber(out, t);
	}
}

// The numbers a hand takes on a frame line after its valid flag and kind.
inline constexpr size_t kHandNumbers = 18;
// valid, kind, 18 numbers, the buttons, curlValid, 5 curls, thumbValid, 3 joints.
inline constexpr size_t kHandTokens = 2 + kHandNumbers + 1 + 1 + 5 + 1 + 3;

inline bool ReadNumber(const std::string& text, float& out) {
	if (text.empty()) {
		return false;
	}
	char* end = nullptr;
	out = std::strtof(text.c_str(), &end);
	return end != nullptr && *end == '\0' && out == out;
}

inline bool ReadFlag(const std::string& text, bool& out) {
	if (text != "0" && text != "1") {
		return false;
	}
	out = text == "1";
	return true;
}

inline bool ReadHand(const std::vector<std::string>& t, size_t at, vr::HandPose& h) {
	if (at + kHandTokens > t.size() || !ReadFlag(t[at], h.valid)) {
		return false;
	}
	const std::string& kind = t[at + 1];
	if (kind != "i" && kind != "o" && kind != "u") {
		return false;
	}
	h.kind = kind == "i" ? vr::ControllerKind::Index : (kind == "o" ? vr::ControllerKind::Other : vr::ControllerKind::Unknown);
	float n[kHandNumbers];
	for (size_t i = 0; i < kHandNumbers; ++i) {
		if (!ReadNumber(t[at + 2 + i], n[i])) {
			return false;
		}
	}
	h.orientation = vr::Quaternion{n[0], n[1], n[2], n[3]};
	h.position = NiPoint3{n[4], n[5], n[6]};
	h.velocity = NiPoint3{n[7], n[8], n[9]};
	h.angularVelocity = NiPoint3{n[10], n[11], n[12]};
	h.trigger = n[13];
	h.thumbX = n[14];
	h.thumbY = n[15];
	h.gripPull = n[16];
	h.gripForce = n[17];
	size_t i = at + 2 + kHandNumbers;
	char* end = nullptr;
	h.buttonsPressed = std::strtoull(t[i].c_str(), &end, 16);
	if (t[i].empty() || end == nullptr || *end != '\0') {
		return false;
	}
	if (!ReadFlag(t[++i], h.curlValid)) {
		return false;
	}
	for (float& c : h.curl) {
		if (!ReadNumber(t[++i], c)) {
			return false;
		}
	}
	if (!ReadFlag(t[++i], h.thumbValid)) {
		return false;
	}
	for (float& j : h.thumb) {
		if (!ReadNumber(t[++i], j)) {
			return false;
		}
	}
	// As the action path reported it: every action bound (the script's own
	// hands are made the same way, test/HandScriptPose.h).
	h.actionInput = true;
	h.actionActiveMask = 0x1FF;
	h.thumbFromJoystickAxis = true;
	return true;
}

}  // namespace detail

// One frame as a hand script line:
//   frame <dt> <head valid> <qx qy qz qw> <px py pz> R <hand> L <hand>
// a hand being: <valid> <i|o|u (Index, other, unknown)> <qx qy qz qw>
// <px py pz> <vx vy vz> <wx wy wz> <trigger> <stick x y> <grip pull>
// <grip force> <buttons, hex> <curl valid> <5 curls> <thumb valid> <3 joints>.
inline std::string PoseSampleLine(const PoseSample& s) {
	std::string out = "frame";
	detail::AppendNumber(out, s.dt);
	out += s.headValid ? " 1" : " 0";
	const float head[] = {s.head.x, s.head.y, s.head.z, s.head.w, s.headPosition.x, s.headPosition.y, s.headPosition.z};
	for (float v : head) {
		detail::AppendNumber(out, v);
	}
	out += " R";
	detail::AppendHand(out, s.right);
	out += " L";
	detail::AppendHand(out, s.left);
	return out;
}

// A `frame` line's tokens (the first being "frame") back into a frame.
inline bool ParsePoseSample(const std::vector<std::string>& t, PoseSample& s) {
	constexpr size_t kHeadTokens = 1 + 1 + 1 + 7;  // frame, dt, valid, 7 numbers
	if (t.size() != kHeadTokens + 1 + detail::kHandTokens + 1 + detail::kHandTokens || t[0] != "frame") {
		return false;
	}
	s = PoseSample{};
	float head[7];
	if (!detail::ReadNumber(t[1], s.dt) || s.dt < 0.0f || !detail::ReadFlag(t[2], s.headValid)) {
		return false;
	}
	for (size_t i = 0; i < 7; ++i) {
		if (!detail::ReadNumber(t[3 + i], head[i])) {
			return false;
		}
	}
	s.head = vr::Quaternion{head[0], head[1], head[2], head[3]};
	s.headPosition = NiPoint3{head[4], head[5], head[6]};
	const size_t right = kHeadTokens;
	const size_t left = right + 1 + detail::kHandTokens;
	return t[right] == "R" && detail::ReadHand(t, right + 1, s.right) && t[left] == "L" &&
	       detail::ReadHand(t, left + 1, s.left);
}

// The whole ring as a runnable hand script: `header` (comment lines, each
// starting with '#'), then marks around the frames so the runner takes
// pictures at both ends and a scenario can hang its console lines on them.
inline std::string PoseRecordingScript(const PoseRing& ring, const std::string& header) {
	std::string out = header;
	out += "\nexpect HandScript: finished\nreject HandScript: WARNING\n\nwait 12\nmark replay_start\n";
	for (const PoseSample& s : ring.samples) {
		out += PoseSampleLine(s);
		out += '\n';
	}
	out += "mark replay_end\nwait 1\n";
	return out;
}

}  // namespace obvr::test
