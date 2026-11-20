#include "core/ConfigDebug.h"

#include "core/Config.h"
#include "core/ConfigRead.h"
#include "core/Log.h"
#include "game/ScriptClockLogic.h"
#include "vr/SneakIndicator.h"

namespace obvr {

void ReadRecordStep(Config& config, const char* path) {
	using config_read::ReadBool;
	using config_read::ReadFloat;
	using config_read::ReadUInt;
	test::RecordSettings& r = config.record;
	r.enabled = ReadBool("Debug", "Record", r.enabled, path);
	r.seconds = ReadFloat("Debug", "RecordSeconds", r.seconds, path);
	r.every = ReadUInt("Debug", "RecordEvery", r.every, path);
	r.width = ReadUInt("Debug", "RecordWidth", r.width, path);
	r.cropLeft = ReadFloat("Debug", "RecordCropLeft", r.cropLeft, path);
	r.cropTop = ReadFloat("Debug", "RecordCropTop", r.cropTop, path);
	r.cropRight = ReadFloat("Debug", "RecordCropRight", r.cropRight, path);
	r.cropBottom = ReadFloat("Debug", "RecordCropBottom", r.cropBottom, path);
}

void ReadDebugTracesStep(Config& config, const char* path) {
	config.scriptTimeTraceMs = game::ScriptClockThresholdMs(
		config_read::ReadFloat("Debug", "ScriptTimeTraceMs", config.scriptTimeTraceMs, path));
	config.flameTrace = config_read::ReadBool("Debug", "FlameTrace", config.flameTrace, path);
	config.handLagTraceFrames = config_read::ReadUInt("Debug", "HandLagTrace", config.handLagTraceFrames, path);
	config.body.wearCheckAsPlayer =
		config_read::ReadBool("Debug", "WearCheckAsPlayer", config.body.wearCheckAsPlayer, path);
}

void ReadSneakIndicatorStep(Config& config, const char* path) {
	vr::HandSettings& h = config.hands;
	char sneakWord[32] = {};
	config_read::ReadText("Hands", "SneakIndicator", sneakWord, sizeof(sneakWord), path);
	if (sneakWord[0] != 0 && !vr::ParseSneakIndicator(sneakWord, h.sneakIndicator)) {
		OBVR_LOG("Config: [Hands] SneakIndicator=%s is not vignette or eye - kept %s", sneakWord,
		         vr::kSneakIndicatorNames[static_cast<UInt32>(h.sneakIndicator)]);
	}
}

void ReadLetGoAndThrowStep(Config& config, const char* path) {
	using config_read::ReadBool;
	using config_read::ReadFloat;
	game::WeaponReleaseSettings& wr = config.hands.weaponRelease;
	wr.enabled = ReadBool("Hands", "WeaponHold", wr.enabled, path);
	wr.releaseSeconds = ReadFloat("Hands", "WeaponReleaseSeconds", wr.releaseSeconds, path);
	wr.throwStrength = ReadFloat("Hands", "WeaponThrowStrength", wr.throwStrength, path);
	wr.sheatheAtOnce = ReadBool("Hands", "SheatheAtOnce", wr.sheatheAtOnce, path);
	game::ThrowHitSettings& th = config.hands.throwHit;
	th.enabled = ReadBool("Hands", "ThrowHits", th.enabled, path);
	th.bounceBack.enabled = ReadBool("Hands", "BounceBack", th.bounceBack.enabled, path);
	th.bounceBack.returnShare = ReadFloat("Hands", "BounceBackShare", th.bounceBack.returnShare, path);
	th.bounceBack.guideSeconds = ReadFloat("Hands", "BounceBackGuideSeconds", th.bounceBack.guideSeconds, path);
}

}  // namespace obvr
