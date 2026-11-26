#include "camera/hands/HandReachRead.h"

#include "core/Log.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/FittedCapsules.h"
#include "platform/Win32Min.h"
#include "test/HandScriptRuntime.h"
#include "vr/FingerTip.h"

namespace obvr::camera::hands {

namespace {

const char* RoleName(int role) { return role == 1 ? "right" : "left"; }

// One role's hand as the bones stand now, in the world: its palm's middle
// and its held-item node's axes - what its skinned vertices are read against.
struct RoleNow {
	bool found = false;
	NiPoint3 palm{0.0f, 0.0f, 0.0f};
	vr::HeldAxes held;
};

RoleNow ReadRole(bool right) {
	RoleNow now;
	const NiAVObject* const wrist = game::FindFirstPersonNode(right ? "Bip01 R Hand" : "Bip01 L Hand");
	const NiAVObject* const middle = game::FindFirstPersonNode(right ? "Bip01 R Finger2" : "Bip01 L Finger2");
	const NiAVObject* const node = game::FindFirstPersonNode(right ? "Weapon" : "Torch");
	if (wrist == nullptr || middle == nullptr || node == nullptr) {
		return now;
	}
	now.found = true;
	now.palm = vr::PalmBetween(wrist->worldTransform.pos, middle->worldTransform.pos);
	now.held.valid = true;
	now.held.blade = node->worldTransform.rot * NiPoint3{0.0f, 1.0f, 0.0f};
	now.held.edge = node->worldTransform.rot * NiPoint3{1.0f, 0.0f, 0.0f};
	return now;
}

// A reading said when it is the first, when it comes or goes, or when it
// has moved two millimetres since the last line - the fingers closing.
// With it, where on the page the foremost point lies on either side: what
// the page rests on.
void SayReading(HandReachState& state, int role, const std::vector<NiPoint3>& vertices, const RoleNow& hand,
                float unitsPerMetre, float indexCurl, const vr::HeldPlace& place) {
	const vr::HandReach& now = state.read[role];
	vr::HandReach& said = state.said[role];
	const auto apart = [](float a, float b) { return a - b > 0.002f || b - a > 0.002f; };
	const bool news = now.valid != said.valid ||
	                  (now.valid && (apart(now.beforeLeft, said.beforeLeft) || apart(now.beforeRight, said.beforeRight)));
	if (!news || state.linesLeft == 0) {
		return;
	}
	--state.linesLeft;
	said = now;
	const UInt32 count = static_cast<UInt32>(vertices.size());
	if (!now.valid) {
		OBVR_LOG("Tablet: the %s hand's model not read (%u vertices%s) - the page stands an open hand's %.1f cm before "
		         "the palm",
		         RoleName(role), count, hand.found ? "" : ", its bones not found",
		         static_cast<double>(vr::kHandReachUnreadMetres * 100.0f));
		return;
	}
	const vr::HandForemost left = vr::ForemostOfHand(vertices.data(), count, hand.palm, hand.held, place, true, unitsPerMetre);
	const vr::HandForemost right =
		vr::ForemostOfHand(vertices.data(), count, hand.palm, hand.held, place, false, unitsPerMetre);
	OBVR_LOG("Tablet: the %s hand as drawn reaches %.1f cm out of the page's flat on a left hand's side (its foremost "
	         "point %.1f cm across the page from the palm's middle, %.1f cm up it) and %.1f cm on a right hand's (%.1f "
	         "across, %.1f up; %u vertices, the index finger's curl %.2f) - the page stands before it, the hand behind",
	         RoleName(role), static_cast<double>(now.beforeLeft * 100.0f), static_cast<double>(left.across * 100.0f),
	         static_cast<double>(left.up * 100.0f), static_cast<double>(now.beforeRight * 100.0f),
	         static_cast<double>(right.across * 100.0f), static_cast<double>(right.up * 100.0f), count,
	         static_cast<double>(indexCurl));
}

// At a hand-script mark with the tablet up: the holding hand's reach as the
// page is placed by it, and what the step cost since the last mark.
void SayAtMark(HandReachState& state, bool holderRight) {
	const vr::HandReach& s = state.shown[holderRight ? 1 : 0];
	const double frames = state.framesSinceMark > 0 ? static_cast<double>(state.framesSinceMark) : 1.0;
	OBVR_LOG("Tablet: at the mark the %s hand holds it - its reach %.1f cm on a left hand's side, %.1f cm on a right "
	         "hand's%s; %u frame(s) up since the last mark, the hand's model read %u time(s), the step %.4f ms a "
	         "frame on average, the worst %.4f ms",
	         RoleName(holderRight ? 1 : 0), static_cast<double>(s.beforeLeft * 100.0f),
	         static_cast<double>(s.beforeRight * 100.0f), s.valid ? "" : " (not read)", state.framesSinceMark,
	         state.readsSinceMark, 1000.0 * state.secondsSinceMark / frames, 1000.0 * state.worstSeconds);
	state.readsSinceMark = 0;
	state.framesSinceMark = 0;
	state.secondsSinceMark = 0.0;
	state.worstSeconds = 0.0;
}

}  // namespace

void StepHandReachRead(HandReachState& state, bool tabletUp, const vr::HandModeResult& hand, float unitsPerMetre,
                       const vr::HeldPlace& place) {
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const long long before = tabletUp ? ReadPerformanceCounter() : 0;
	const bool holderRight = hand.tabletHolderRight;
	const float* const fingers = holderRight ? hand.rightCurl : hand.leftCurl;
	const float* const thumb = holderRight ? hand.rightThumb : hand.leftThumb;
	float curls[vr::kHandReachCurls];
	for (int i = 0; i < 5; ++i) {
		curls[i] = fingers[i];
	}
	for (int i = 0; i < 3; ++i) {
		curls[5 + i] = thumb[i];
	}
	const bool told = holderRight ? hand.rightCurlValid : hand.leftCurlValid;
	if (vr::StepHandReachClock(state.clock, tabletUp, told ? curls : nullptr)) {
		const NiAVObject* const arms = game::FirstPersonArmsNode();
		const bool got =
			arms != nullptr && game::ReadHandVerticesMeasured(arms, state.vertices[0], state.vertices[1]) > 0;
		for (int role = 0; role < 2; ++role) {
			// The right role's hand hangs on the R bones, whichever controller holds it.
			const std::vector<NiPoint3>& vertices = state.vertices[role == 1 ? 0 : 1];
			const RoleNow now = got ? ReadRole(role == 1) : RoleNow{};
			state.read[role] = vr::ReachOfHand(vertices.data(), static_cast<UInt32>(vertices.size()), now.palm,
			                                   now.held, place, unitsPerMetre);
			SayReading(state, role, vertices, now, unitsPerMetre, role == 1 ? hand.rightCurl[1] : hand.leftCurl[1], place);
		}
		++state.readsSinceMark;
	}
	if (!tabletUp) {
		return;
	}
	vr::StepHandReach(state.shown[0], state.read[0]);
	vr::StepHandReach(state.shown[1], state.read[1]);
	if (ticksPerSecond > 0) {
		const double seconds = static_cast<double>(ReadPerformanceCounter() - before) / static_cast<double>(ticksPerSecond);
		state.secondsSinceMark += seconds;
		state.worstSeconds = seconds > state.worstSeconds ? seconds : state.worstSeconds;
		++state.framesSinceMark;
	}
	if (test::HandScriptMarkedThisFrame()) {
		SayAtMark(state, holderRight);
	}
}

}  // namespace obvr::camera::hands
