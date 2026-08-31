#include "ui/SettingsList.h"

namespace obvr::ui {

namespace {

// Captureless lambdas so that each row's reach into the configuration sits on
// the row itself. Naming a member twice - once to read, once to write - is what
// makes the "no row disturbs another" test able to find a mistake at all: there
// is nothing else to compare against.
const SettingDefinition kSettings[] = {
	// ---- Comfort -----------------------------------------------------------
	{
		"Comfort", "Head movement", "How far leaning in the room moves the camera",
		ItemKind::Number, 0.5f, 6.0f, 0.25f, 2, false,
		+[](const Config& c) { return c.tracker.movementScale; },
		+[](Config& c, float v) { c.tracker.movementScale = v; },
	},
	{
		"Comfort", "Eye separation", "Wider feels smaller, narrower feels larger",
		ItemKind::Number, 0.7f, 1.3f, 0.01f, 2, false,
		+[](const Config& c) { return c.tracker.eyeSeparationScale; },
		+[](Config& c, float v) { c.tracker.eyeSeparationScale = v; },
	},
	{
		"Comfort", "Smooth turning", "Eases the mouse turn instead of snapping",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.look.smoothTurning ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.smoothTurning = v != 0.0f; },
	},
	{
		"Comfort", "Turn speed", "How fast an eased turn catches up",
		ItemKind::Number, 1.0f, 30.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.look.turnSpeed; },
		+[](Config& c, float v) { c.look.turnSpeed = v; },
	},

	// ---- Looking -----------------------------------------------------------
	{
		"Looking", "Head steers the view", "Vertical look comes from the head, not the mouse",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.look.blockVerticalLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.blockVerticalLook = v != 0.0f; },
	},
	{
		"Looking", "Look up range", "Degrees of upward look, third person only",
		ItemKind::Number, 0.0f, 180.0f, 5.0f, 0, false,
		+[](const Config& c) { return c.look.verticalLookUpRange; },
		+[](Config& c, float v) { c.look.verticalLookUpRange = v; },
	},
	{
		"Looking", "Look down range", "Degrees of downward look, third person only",
		ItemKind::Number, 0.0f, 180.0f, 5.0f, 0, false,
		+[](const Config& c) { return c.look.verticalLookDownRange; },
		+[](Config& c, float v) { c.look.verticalLookDownRange = v; },
	},
	{
		"Looking", "Smooth vertical look", "Eases the vertical look as well",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.look.smoothVerticalLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.smoothVerticalLook = v != 0.0f; },
	},

	// ---- Aiming ------------------------------------------------------------
	{
		"Aiming", "Aim follows gaze", "Shots leave where you are looking",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.aimFollowsGaze ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.aimFollowsGaze = v != 0.0f; },
	},
	{
		"Aiming", "Body turn speed", "0 turns the body at once; higher eases it",
		ItemKind::Number, 0.0f, 30.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.aimTurnSpeed; },
		+[](Config& c, float v) { c.aimTurnSpeed = v; },
	},
	{
		"Aiming", "Crosshair", "Oblivion's crosshair at its own depth",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.tracker.crosshair ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshair = v != 0.0f; },
	},
	{
		"Aiming", "Crosshair distance", "Metres. Where it hangs in front of you",
		ItemKind::Number, 0.5f, 30.0f, 0.5f, 1, false,
		+[](const Config& c) { return c.tracker.crosshairDistanceMetres; },
		+[](Config& c, float v) { c.tracker.crosshairDistanceMetres = v; },
	},
	{
		"Aiming", "Crosshair size", "Metres wide at one metre away",
		ItemKind::Number, 0.005f, 0.15f, 0.005f, 3, false,
		+[](const Config& c) { return c.tracker.crosshairSizeAtOneMetre; },
		+[](Config& c, float v) { c.tracker.crosshairSizeAtOneMetre = v; },
	},

	// ---- Screen and menus --------------------------------------------------
	{
		"Screen", "HUD distance", "Metres. How far away the flat picture hangs",
		ItemKind::Number, 0.4f, 8.0f, 0.1f, 1, false,
		+[](const Config& c) { return c.tracker.hudDistanceMetres; },
		+[](Config& c, float v) { c.tracker.hudDistanceMetres = v; },
	},
	{
		"Screen", "HUD width", "Metres. How wide it is at that distance",
		ItemKind::Number, 0.4f, 8.0f, 0.1f, 1, false,
		+[](const Config& c) { return c.tracker.hudWidthMetres; },
		+[](Config& c, float v) { c.tracker.hudWidthMetres = v; },
	},
	{
		"Screen", "Menu scale", "How much of the picture a menu fills",
		ItemKind::Number, 0.3f, 2.0f, 0.05f, 2, false,
		+[](const Config& c) { return c.tracker.menuScale; },
		+[](Config& c, float v) { c.tracker.menuScale = v; },
	},
	{
		"Screen", "Menus in the world", "Menus hang in front of you instead of on the screen",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.tracker.menusInWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.menusInWorld = v != 0.0f; },
	},
	{
		"Screen", "Menu shade", "The vanilla brown wash behind a menu",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.tracker.menuShade ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.menuShade = v != 0.0f; },
	},
	{
		"Screen", "Shade strength", "How strong that wash is",
		ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false,
		+[](const Config& c) { return c.tracker.menuShadeStrength; },
		+[](Config& c, float v) { c.tracker.menuShadeStrength = v; },
	},
	{
		"Screen", "Field of view", "Degrees. 0 leaves the game's own",
		ItemKind::Number, 0.0f, 140.0f, 1.0f, 0, false,
		+[](const Config& c) { return c.tracker.gameFovOverride; },
		+[](Config& c, float v) { c.tracker.gameFovOverride = v; },
	},

	// ---- Needs a restart ---------------------------------------------------
	//
	// Kept last and marked. These are read while the device is being created,
	// so changing one here has no effect until the game starts again - and
	// somebody who changes a setting and sees nothing happen concludes the menu
	// does not work, not that this particular row is different.
	{
		"Restart needed", "Render width", "Pixels per eye. 0 asks the headset",
		ItemKind::Number, 0.0f, 8192.0f, 64.0f, 0, true,
		+[](const Config& c) { return static_cast<float>(c.tracker.renderWidth); },
		+[](Config& c, float v) { c.tracker.renderWidth = static_cast<UInt32>(v); },
	},
	{
		"Restart needed", "Render height", "Pixels per eye. 0 asks the headset",
		ItemKind::Number, 0.0f, 8192.0f, 64.0f, 0, true,
		+[](const Config& c) { return static_cast<float>(c.tracker.renderHeight); },
		+[](Config& c, float v) { c.tracker.renderHeight = static_cast<UInt32>(v); },
	},
};

float Clamp(const SettingDefinition& definition, float value) {
	if (value < definition.minimum) {
		return definition.minimum;
	}
	if (value > definition.maximum) {
		return definition.maximum;
	}
	return value;
}

}  // namespace

const SettingDefinition* SettingDefinitions() { return kSettings; }

UInt32 SettingDefinitionCount() {
	return static_cast<UInt32>(sizeof(kSettings) / sizeof(kSettings[0]));
}

MenuItem ItemFor(const SettingDefinition& definition, const Config& config) {
	MenuItem item;
	item.label = definition.label;
	item.help = definition.help;
	item.kind = definition.kind;
	item.minimum = definition.minimum;
	item.maximum = definition.maximum;
	item.step = definition.step;
	item.decimals = definition.decimals;
	item.needsRestart = definition.needsRestart;

	// A row with no reader is a half-finished table entry. Showing it at its
	// minimum makes that visible in the menu; reading through the null pointer
	// would make it visible in a crash dump.
	item.value = definition.Read != nullptr ? definition.Read(config) : definition.minimum;

	// A value already outside its range - carried in from a hand-edited INI -
	// is shown as what it is rather than silently corrected, because correcting
	// it here would write it back on the next frame without anyone asking. The
	// clamp belongs on the way in from a keypress, which is where it is.
	return item;
}

void ApplySetting(const SettingDefinition& definition, Config& config, float value) {
	if (definition.Write == nullptr) {
		return;
	}
	definition.Write(config, Clamp(definition, value));
}

}  // namespace obvr::ui
