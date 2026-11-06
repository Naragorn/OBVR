#include "game/GrassSettings.h"

#include "core/Log.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// A setting object as the EXE lays it out: the value, then the name.
struct Setting {
	union {
		float f;
		SInt32 i;
	};
	const char* name;
};

Setting* SettingAt(UInt32 address) { return reinterpret_cast<Setting*>(address); }

// Whether the object at `address` carries `expected` as its name. The name's
// pointer is checked for reading first: on another build of the EXE these
// addresses hold something else.
bool Named(UInt32 address, const char* expected, UInt32 length) {
	if (IsBadReadPtr(SettingAt(address), sizeof(Setting)) != 0) {
		return false;
	}
	const char* const name = SettingAt(address)->name;
	return name != nullptr && IsBadReadPtr(name, length + 1) == 0 && SettingNameIs(name, expected);
}

GrassNow ReadNow() {
	GrassNow now;
	now.end = SettingAt(kGrassEndSetting)->f;
	now.start = SettingAt(kGrassStartFadeSetting)->f;
	now.minSize = SettingAt(kGrassMinSizeSetting)->i;
	now.buffersUnmade =
		GrassBuffersUnmade(*reinterpret_cast<const UInt32*>(kGrassBufferUsers),
	                       *reinterpret_cast<const UInt32*>(kGrassCellBuffer),
	                       *reinterpret_cast<const UInt32*>(kGrassCountBuffer));
	return now;
}

// The three objects believed once: their names as read from the EXE, their
// values the kind the grass's settings hold.
bool Believe(GrassSettingsState& state) {
	if (state.trust != GrassTrust::Unread) {
		return state.trust == GrassTrust::Believed;
	}
	const bool named = Named(kGrassStartFadeSetting, kGrassStartFadeName, sizeof(kGrassStartFadeName) - 1) &&
	                   Named(kGrassEndSetting, kGrassEndName, sizeof(kGrassEndName) - 1) &&
	                   Named(kGrassMinSizeSetting, kGrassMinSizeName, sizeof(kGrassMinSizeName) - 1) &&
	                   IsBadReadPtr(reinterpret_cast<const void*>(kGrassCellBuffer), 12) == 0;
	if (!named) {
		state.trust = GrassTrust::Refused;
		OBVR_LOG("Grass settings: the game's [Grass] settings are not where they were read (%08X, %08X, %08X) - "
		         "Render.GrassDistance and GrassDensity do nothing",
		         kGrassStartFadeSetting, kGrassEndSetting, kGrassMinSizeSetting);
		return false;
	}
	const GrassNow now = ReadNow();
	if (!GrassValuesPlausible(now)) {
		state.trust = GrassTrust::Refused;
		OBVR_LOG("Grass settings: the game's [Grass] settings read as no grass settings (end %.1f, start %.1f, "
		         "iMinGrassSize %d) - Render.GrassDistance and GrassDensity do nothing",
		         static_cast<double>(now.end), static_cast<double>(now.start), now.minSize);
		return false;
	}
	state.trust = GrassTrust::Believed;
	OBVR_LOG("Grass settings: the game holds fGrassEndDistance %.0f, fGrassStartFadeDistance %.0f, iMinGrassSize %d "
	         "(its buffers %s)",
	         static_cast<double>(now.end), static_cast<double>(now.start), now.minSize,
	         now.buffersUnmade ? "not made yet" : "made");
	return true;
}

}  // namespace

void StepGrassSettings(GrassSettingsState& state, float wantedDistance, float wantedDensity, bool marked) {
	if (!Believe(state)) {
		return;
	}
	const GrassNow now = ReadNow();
	const GrassWrite write = GrassWriteFor(wantedDistance, wantedDensity, now);
	if (write.distance) {
		SettingAt(kGrassEndSetting)->f = write.end;
		SettingAt(kGrassStartFadeSetting)->f = write.start;
		if (state.linesLeft > 0) {
			--state.linesLeft;
			OBVR_LOG("Grass settings: the reach set to %.0f units, the fade from %.0f (Render.GrassDistance %.0f; the "
			         "game held %.0f and %.0f) - from the grass's next build",
			         static_cast<double>(write.end), static_cast<double>(write.start),
			         static_cast<double>(wantedDistance), static_cast<double>(now.end),
			         static_cast<double>(now.start));
		}
	}
	if (write.minSize) {
		SettingAt(kGrassMinSizeSetting)->i = write.size;
		state.waitSaid = false;
		if (state.linesLeft > 0) {
			--state.linesLeft;
			OBVR_LOG("Grass settings: iMinGrassSize set to %d (Render.GrassDensity %.0f %%; the game held %d) - its "
			         "buffers are not made yet, the grass is placed with it",
			         write.size, static_cast<double>(wantedDensity), now.minSize);
		}
	} else if (write.minSizeWaits && (!state.waitSaid || state.waitSize != write.size)) {
		state.waitSaid = true;
		state.waitSize = write.size;
		OBVR_LOG("Grass settings: Render.GrassDensity %.0f %% wants iMinGrassSize %d, the grass's buffers stand for "
		         "%d - it holds from the next start",
		         static_cast<double>(wantedDensity), write.size, now.minSize);
	}
	if (marked) {
		const GrassNow after = ReadNow();
		OBVR_LOG("Grass settings: at the mark - the end %.0f, the fade from %.0f, iMinGrassSize %d, the buffers %s",
		         static_cast<double>(after.end), static_cast<double>(after.start), after.minSize,
		         after.buffersUnmade ? "not made" : "made");
	}
}

}  // namespace obvr::game
