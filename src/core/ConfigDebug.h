#pragma once

// [Debug] keys read as steps of the key table (core/Config.cpp), their keys
// named there for the INI audit: the recording mode (Record and its keys,
// test/RecordingPlan.h, render/EyeRecorder.h) and the script clock's
// threshold (ScriptTimeTraceMs, game/ScriptClockLogic.h).

namespace obvr {

struct Config;

void ReadRecordStep(Config& config, const char* path);
// The script clock's threshold (ScriptTimeTraceMs) and FlameTrace (game/FlameTrace.h).
void ReadDebugTracesStep(Config& config, const char* path);
// [Hands] SneakIndicator: a word, vignette or eye (vr/SneakIndicator.h).
void ReadSneakIndicatorStep(Config& config, const char* path);
// [Hands]: the drawn weapon let go of and thrown things - WeaponHold,
// WeaponReleaseSeconds, WeaponThrowStrength, SheatheAtOnce (game/WeaponRelease.h),
// ThrowHits and the bounce back, BounceBack, BounceBackShare,
// BounceBackGuideSeconds (game/ThrowLogic.h, game/BounceBackLogic.h).
void ReadLetGoAndThrowStep(Config& config, const char* path);

}  // namespace obvr
