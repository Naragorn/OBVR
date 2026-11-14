#include "camera/hands/Hints.h"

#include "platform/Win32Min.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "camera/hands/OverlayPose.h"
#include "core/Log.h"
#include "platform/PluginPath.h"
#include "render/GameDevice.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {
namespace {

constexpr const char* kHintsFile = "OBVR_Hints.ini";
// The settings' request to show the hints again, owned here until the next step.
bool& ResetRequested() {
	static bool requested = false;
	return requested;
}

UInt32 ReadCount(const char* key, const char* path) {
	char buffer[16]{};
	GetPrivateProfileStringA("Hints", key, "0", buffer, sizeof(buffer), path);
	return static_cast<UInt32>(std::strtoul(buffer, nullptr, 10));
}

void Load(HintsState& state) {
	state.loaded = true;
	state.harness = HarnessRunActive();
	if (state.harness) {
		OBVR_LOG("Hints: a harness run - %s neither read nor written, every hint new", kHintsFile);
		return;
	}
	char path[512];
	if (!platform::BuildPluginPath(kHintsFile, path, sizeof(path))) {
		return;
	}
	char key[64];
	UInt32 learned = 0;
	for (UInt32 i = 0; i < vr::kHintCount; ++i) {
		std::snprintf(key, sizeof(key), "%sShown", vr::HintName(i));
		state.logic.memory.shown[i] = ReadCount(key, path);
		std::snprintf(key, sizeof(key), "%sLearned", vr::HintName(i));
		state.logic.memory.learned[i] = ReadCount(key, path) != 0;
		learned += state.logic.memory.learned[i] ? 1 : 0;
	}
	OBVR_LOG("Hints: %u of %u learned (%s)", learned, vr::kHintCount, kHintsFile);
}

// Written only when a showing starts or a hint is learned: a few times in all.
void Save(const HintsState& state) {
	char path[512];
	if (state.harness || !platform::BuildPluginPath(kHintsFile, path, sizeof(path))) {
		return;
	}
	char key[64];
	char value[16];
	bool written = true;
	for (UInt32 i = 0; i < vr::kHintCount; ++i) {
		std::snprintf(key, sizeof(key), "%sShown", vr::HintName(i));
		std::snprintf(value, sizeof(value), "%u", state.logic.memory.shown[i]);
		written = WritePrivateProfileStringA("Hints", key, value, path) != 0 && written;
		std::snprintf(key, sizeof(key), "%sLearned", vr::HintName(i));
		written = WritePrivateProfileStringA("Hints", key, state.logic.memory.learned[i] ? "1" : "0", path) != 0 &&
		          written;
	}
	if (!written) {
		static bool said = false;
		if (!said) {
			said = true;
			OBVR_LOG("Hints: COULD NOT WRITE %s - the hints will come again next start", path);
		}
	}
}

}  // namespace

bool AnyHintOpen(HintsState& state, const Config& config) {
	if (!config.onboardingHints) {
		return false;
	}
	if (!state.loaded) {
		Load(state);
	}
	const vr::HintSettings settings;
	for (UInt32 i = 0; i < vr::kHintCount; ++i) {
		if (vr::HintOpen(settings, state.logic.memory, i)) {
			return true;
		}
	}
	return state.logic.active >= 0;
}

void StepHintSigns(HintsState& state, const HandStep& step, const vr::HandModeFrame& frame, const HintLinks& links) {
	if (!state.loaded) {
		Load(state);
	}
	if (ResetRequested()) {
		ResetRequested() = false;
		state.logic = vr::HintState{};
		Save(state);
		OBVR_LOG("Hints: shown again - every hint new");
	}
	vr::HintSettings settings;
	settings.enabled = step.config.onboardingHints;
	vr::HintFrame in;
	for (UInt32 i = 0; i < vr::kHintCount; ++i) {
		in.wants[i] = links.wants[i];
		in.did[i] = links.did[i];
	}
	in.quiet = !step.active || step.menuIsUp || !frame.inWorld || !frame.headValid;
	in.dt = step.dt;
	const vr::HintStep out = vr::StepHints(state.logic, settings, in);
	if (out.learnedNow && state.lines > 0) {
		--state.lines;
		for (UInt32 i = 0; i < vr::kHintCount; ++i) {
			if (links.did[i]) {
				OBVR_LOG("Hints: %s done - learned, not shown again", vr::HintName(i));
			}
		}
	}
	if (out.memoryChanged) {
		Save(state);
	}
	vr::OpenVRBackend& backend = step.backend;
	if (out.show < 0) {
		state.signLayer.Hide(backend);
		return;
	}
	const UInt32 hint = static_cast<UInt32>(out.show);
	// The weapon hand is the right one unless the roles are swapped.
	const bool rightHand =
		links.hand[hint] != 0 ? links.hand[hint] == 2 : vr::HintOnWeaponHand(hint) != frame.rolesSwapped;
	const vr::HandPose& pose = rightHand ? frame.right : frame.left;
	if (!pose.valid) {
		state.signLayer.Hide(backend);
		return;
	}
	if (state.viewFor != out.show) {
		const vr::HintWords words = vr::HintWordsFor(hint);
		state.view = ui::HolsterSignView{};
		std::snprintf(state.view.item, sizeof(state.view.item), "%s", words.title);
		std::snprintf(state.view.hint, sizeof(state.view.hint), "%s", words.line);
		state.viewFor = out.show;
		++state.revision;
	}
	const NiPoint3 above{pose.position.x, pose.position.y + kHintSignAboveMetres, pose.position.z};
	state.signLayer.Show(backend, render::GetGameDevice(), StowSpotPose(above, frame.headPosition), kHintSignMetres,
	                     state.revision, ui::PaintHolsterSignFor, &state.view);
	if (out.started && state.lines > 0) {
		--state.lines;
		OBVR_LOG("Hints: \"%s\" shown over the %s hand (showing %u of %u)", state.view.item,
		         rightHand ? "right" : "left", state.logic.memory.shown[hint], settings.maxShows);
	}
	// For the test runner: the sign as painted, at each mark it is up for.
	if (step.scriptMarked && state.signLayer.IsVisible()) {
		char name[96];
		char path[512];
		std::snprintf(name, sizeof(name), "OBVR-HintSign-%s-%s.bmp", vr::HintName(hint), test::HandScriptMarkName());
		if (platform::BuildGamePath(name, path, sizeof(path)) && state.signLayer.SaveBmp(path)) {
			OBVR_LOG("HandScript: the hint sign's picture saved as %s", name);
		}
	}
}

void RequestHintsReset() { ResetRequested() = true; }

void HideHintSign(HintsState& state, vr::OpenVRBackend& backend) {
	state.logic.active = -1;
	state.signLayer.Hide(backend);
}

}  // namespace obvr::camera::hands
