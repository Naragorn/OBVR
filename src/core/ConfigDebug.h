#pragma once

// [Debug] keys read as steps of the key table (core/Config.cpp), their keys
// named there for the INI audit: the recording mode (Record and its keys,
// test/RecordingPlan.h, render/EyeRecorder.h) and the script clock's
// threshold (ScriptTimeTraceMs, game/ScriptClockLogic.h).

namespace obvr {

struct Config;

void ReadRecordStep(Config& config, const char* path);
void ReadScriptClockThresholdStep(Config& config, const char* path);

}  // namespace obvr
