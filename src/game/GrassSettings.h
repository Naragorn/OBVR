#pragma once

// The grass's reach and density as OBVR settings ([Render] GrassDistance and
// GrassDensity; Nadi, 2026-11-06). Both are the game's own [Grass] settings
// of Oblivion.ini, set in the game's memory.
//
// Read from the EXE's instructions, 2026-11-06 (objdump; the setting objects
// found first by the OBNG session, OBNG knowledge/culling-and-draw-setup.md):
// - a setting object is eight bytes, the value and then the name's pointer:
//   fGrassStartFadeDistance at 0x00B09B10 (2000), fGrassEndDistance at
//   0x00B09B18 (3000), iMinGrassSize at 0x00B09B20 (80);
// - the grass's build (0x004EBDA0, the function game/GrassCone.h watches)
//   loads the end and the start from the two settings on every call
//   (0x004EBDFC, 0x004EBE12) and hands them on (0x007C2960). The game's own
//   video options (0x005DF3E1 ...) do nothing more than write the two: the
//   end is its slider's 0..100 over 100 times 7600 plus 400, the start the
//   end less 1000 and not below 0; an end of 400.5 or less sets both to 0
//   and takes the grass away (0x007C4CE0). So a reach written here holds
//   from the next build, as the game's own slider's does, and 8000 is the
//   most that slider sets;
// - iMinGrassSize sizes two buffers, (0x800 / iMinGrassSize) squared entries
//   each ([0x00B36098] twelve bytes an entry, [0x00B3609C] four), allocated
//   in the constructor at 0x004EA450 when its use count ([0x00B360A0]) is 0
//   and the buffer is not there yet; the placing (0x004EA9B8) divides by the
//   setting again. A smaller spacing set while those buffers stand would
//   ask more of them than they hold. So the spacing is written only while
//   there is no user and neither buffer - before the first grass of a
//   session - and otherwise waits for the next start.
//
// The density in percent is the places per area against the game's default
// spacing of 80: the places go with the buffers' entries, one over the
// spacing squared (derived from the sizing above, not counted in the game).
//
// What is written stays the game's: OBVR never puts the old value back, and
// the game writes its settings to Oblivion.ini when it saves them, as after
// its own video options (not at the end of a harness run: Oblivion.ini was
// the same file after four of them, 2026-11-06). 0 leaves a setting to the
// game.
//
// Measured 2026-11-06 (tools/hand-scripts/grass-settings.txt): the settings
// are first read in the main menu, the buffers not made yet, so the spacing
// goes in before a save's grass; a reach of 2000 units left 39 patches under
// the Grass node, the game's 3000 49, and 8000 set in the running game 146
// once the head had turned and the grass was built anew; a density asked
// for after that waited. What the density looks like was not measured.
//
// Pure apart from StepGrassSettings; covered by grass_settings_test.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kGrassStartFadeSetting = 0x00B09B10;
inline constexpr UInt32 kGrassEndSetting = 0x00B09B18;
inline constexpr UInt32 kGrassMinSizeSetting = 0x00B09B20;
inline constexpr UInt32 kGrassCellBuffer = 0x00B36098;
inline constexpr UInt32 kGrassCountBuffer = 0x00B3609C;
inline constexpr UInt32 kGrassBufferUsers = 0x00B360A0;

inline constexpr char kGrassStartFadeName[] = "fGrassStartFadeDistance:Grass";
inline constexpr char kGrassEndName[] = "fGrassEndDistance:Grass";
inline constexpr char kGrassMinSizeName[] = "iMinGrassSize:Grass";

// The reach: the game's own slider goes from 400 to 8000 and fades over the
// last 1000; under 400.5 the game takes the grass away, which is the game's
// own switch and not this setting's.
inline constexpr float kGrassDistanceLeast = 1000.0f;
inline constexpr float kGrassDistanceMost = 8000.0f;
inline constexpr float kGrassFadeBand = 1000.0f;

// The density: a quarter to four times the game's places per area.
inline constexpr float kGrassDensityLeast = 25.0f;
inline constexpr float kGrassDensityMost = 400.0f;
inline constexpr SInt32 kGrassMinSizeDefault = 80;

struct GrassDistances {
	bool set = false;
	float end = 0.0f;
	float start = 0.0f;
};

// The end and the fade's start for a wanted reach in units: held within the
// least and the most, the start a fade band before the end. 0, less or no
// number: nothing set.
inline GrassDistances GrassDistancesFor(float wanted) {
	GrassDistances d;
	if (!(wanted > 0.0f)) {
		return d;
	}
	d.set = true;
	d.end = wanted < kGrassDistanceLeast ? kGrassDistanceLeast : (wanted > kGrassDistanceMost ? kGrassDistanceMost : wanted);
	d.start = d.end > kGrassFadeBand ? d.end - kGrassFadeBand : 0.0f;
	return d;
}

// The game's iMinGrassSize for a density in percent of its default places
// per area: 80 over the root of the share, rounded. 0, less or no number: 0,
// nothing set.
inline SInt32 GrassMinSizeFor(float densityPercent) {
	if (!(densityPercent > 0.0f)) {
		return 0;
	}
	const float percent = densityPercent < kGrassDensityLeast
	                          ? kGrassDensityLeast
	                          : (densityPercent > kGrassDensityMost ? kGrassDensityMost : densityPercent);
	// The root by Newton's steps: no CRT in a header shared with the tests.
	const float share = percent / 100.0f;
	float root = share > 1.0f ? share : 1.0f;
	for (int i = 0; i < 24; ++i) {
		root = 0.5f * (root + share / root);
	}
	return static_cast<SInt32>(static_cast<float>(kGrassMinSizeDefault) / root + 0.5f);
}

// Whether the buffers iMinGrassSize sizes are still unmade: no user and
// neither buffer there.
inline bool GrassBuffersUnmade(UInt32 users, UInt32 cellBuffer, UInt32 countBuffer) {
	return users == 0 && cellBuffer == 0 && countBuffer == 0;
}

// What the game holds now.
struct GrassNow {
	float end = 0.0f;
	float start = 0.0f;
	SInt32 minSize = 0;
	bool buffersUnmade = false;
};

// Whether those read like the grass's settings: the distances numbers from 0
// to a million units, the spacing from 1 to the 2048 the game divides by it.
inline bool GrassValuesPlausible(const GrassNow& now) {
	return now.end >= 0.0f && now.end <= 1.0e6f && now.start >= 0.0f && now.start <= 1.0e6f && now.minSize >= 1 &&
	       now.minSize <= 0x800;
}

// Whether a setting object's name is the one expected, to the end.
inline bool SettingNameIs(const char* name, const char* expected) {
	if (name == nullptr || expected == nullptr) {
		return false;
	}
	UInt32 i = 0;
	for (; expected[i] != '\0'; ++i) {
		if (name[i] != expected[i]) {
			return false;
		}
	}
	return name[i] == '\0';
}

struct GrassWrite {
	bool distance = false;  // write end and start
	float end = 0.0f;
	float start = 0.0f;
	bool minSize = false;  // write size
	SInt32 size = 0;
	bool minSizeWaits = false;  // the spacing differs but the buffers stand: the next start
};

// What to write for the wanted reach (units) and density (percent) against
// what the game holds: the distances when either differs; the spacing when
// it differs and the buffers are unmade - made, it waits.
inline GrassWrite GrassWriteFor(float wantedDistance, float wantedDensity, const GrassNow& now) {
	GrassWrite w;
	const GrassDistances d = GrassDistancesFor(wantedDistance);
	if (d.set && (d.end != now.end || d.start != now.start)) {
		w.distance = true;
		w.end = d.end;
		w.start = d.start;
	}
	const SInt32 size = GrassMinSizeFor(wantedDensity);
	if (size != 0 && size != now.minSize) {
		w.size = size;
		w.minSize = now.buffersUnmade;
		w.minSizeWaits = !now.buffersUnmade;
	}
	return w;
}

// What the step carries from frame to frame; its caller owns it.
enum class GrassTrust : UInt8 { Unread, Believed, Refused };

struct GrassSettingsState {
	GrassTrust trust = GrassTrust::Unread;
	UInt32 linesLeft = 12;
	bool waitSaid = false;
	SInt32 waitSize = 0;
};

// Once a frame, from the interface's render (which runs in the main menu
// too, before a save's grass is made): reads the three settings, refuses
// for good when their names or values are not as read, and writes what
// GrassWriteFor says. Each write and the spacing's wait are said in the log,
// and the values at a hand script's mark.
void StepGrassSettings(GrassSettingsState& state, float wantedDistance, float wantedDensity, bool marked);

}  // namespace obvr::game
