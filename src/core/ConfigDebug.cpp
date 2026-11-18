#include "core/ConfigDebug.h"

#include "core/Config.h"
#include "core/ConfigRead.h"
#include "game/ScriptClockLogic.h"

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

void ReadScriptClockThresholdStep(Config& config, const char* path) {
	config.scriptTimeTraceMs = game::ScriptClockThresholdMs(
		config_read::ReadFloat("Debug", "ScriptTimeTraceMs", config.scriptTimeTraceMs, path));
}

}  // namespace obvr
