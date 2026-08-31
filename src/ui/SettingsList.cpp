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
		"Head", "HeadMovementScale",
		+[](const Config& c) { return c.tracker.movementScale; },
		+[](Config& c, float v) { c.tracker.movementScale = v; },
	},
	{
		"Comfort", "Eye separation", "Wider feels smaller, narrower feels larger",
		ItemKind::Number, 0.7f, 1.3f, 0.01f, 2, false,
		"Render", "EyeSeparationScale",
		+[](const Config& c) { return c.tracker.eyeSeparationScale; },
		+[](Config& c, float v) { c.tracker.eyeSeparationScale = v; },
	},
	{
		"Comfort", "Smooth turning", "Eases the mouse turn instead of snapping",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "SmoothTurning",
		+[](const Config& c) { return c.look.smoothTurning ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.smoothTurning = v != 0.0f; },
	},
	{
		"Comfort", "Turn speed", "How fast an eased turn catches up",
		ItemKind::Number, 1.0f, 30.0f, 1.0f, 0, false,
		"Look", "TurnSpeed",
		+[](const Config& c) { return c.look.turnSpeed; },
		+[](Config& c, float v) { c.look.turnSpeed = v; },
	},
	{
		"Comfort", "Room tracking", "Leaning in the room moves the camera",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Head", "PositionalTracking",
		+[](const Config& c) { return c.tracker.positionalTracking ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.positionalTracking = v != 0.0f; },
	},
	{
		"Comfort", "Lean limit", "Oblivion units the camera may leave the head at",
		ItemKind::Number, 0.0f, 400.0f, 10.0f, 0, false,
		"Head", "MaxLeanUnits",
		+[](const Config& c) { return c.tracker.maxOffsetUnits; },
		+[](Config& c, float v) { c.tracker.maxOffsetUnits = v; },
	},

	// ---- Looking -----------------------------------------------------------
	{
		"Looking", "Head steers the view", "Vertical look comes from the head, not the mouse",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "BlockVerticalLook",
		+[](const Config& c) { return c.look.blockVerticalLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.blockVerticalLook = v != 0.0f; },
	},
	{
		"Looking", "Look up range", "Degrees of upward look, third person only",
		ItemKind::Number, 0.0f, 180.0f, 5.0f, 0, false,
		"Look", "VerticalLookUpRange",
		+[](const Config& c) { return c.look.verticalLookUpRange; },
		+[](Config& c, float v) { c.look.verticalLookUpRange = v; },
	},
	{
		"Looking", "Look down range", "Degrees of downward look, third person only",
		ItemKind::Number, 0.0f, 180.0f, 5.0f, 0, false,
		"Look", "VerticalLookDownRange",
		+[](const Config& c) { return c.look.verticalLookDownRange; },
		+[](Config& c, float v) { c.look.verticalLookDownRange = v; },
	},
	{
		"Looking", "Smooth vertical look", "Eases the vertical look as well",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "SmoothVerticalLook",
		+[](const Config& c) { return c.look.smoothVerticalLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.smoothVerticalLook = v != 0.0f; },
	},
	{
		"Looking", "Vertical look speed", "How fast the eased vertical look catches up",
		ItemKind::Number, 1.0f, 30.0f, 1.0f, 0, false,
		"Look", "VerticalLookSpeed",
		+[](const Config& c) { return c.look.verticalLookSpeed; },
		+[](Config& c, float v) { c.look.verticalLookSpeed = v; },
	},

	// ---- Aiming ------------------------------------------------------------
	{
		"Aiming", "Aim follows gaze", "Shots leave where you are looking",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "AimFollowsGaze",
		+[](const Config& c) { return c.aimFollowsGaze ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.aimFollowsGaze = v != 0.0f; },
	},
	{
		"Aiming", "Body turn speed", "0 turns the body at once; higher eases it",
		ItemKind::Number, 0.0f, 30.0f, 1.0f, 0, false,
		"Look", "AimTurnSpeed",
		+[](const Config& c) { return c.aimTurnSpeed; },
		+[](Config& c, float v) { c.aimTurnSpeed = v; },
	},
	{
		"Aiming", "Crosshair", "Oblivion's crosshair at its own depth",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "Crosshair",
		+[](const Config& c) { return c.tracker.crosshair ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshair = v != 0.0f; },
	},
	{
		"Aiming", "Crosshair depth", "Follows what you are aiming at, within reach",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "CrosshairDynamic",
		+[](const Config& c) { return c.tracker.crosshairDynamic ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshairDynamic = v != 0.0f; },
		"fixed", "follows",
	},
	{
		"Aiming", "Crosshair distance", "Metres. Where it hangs with nothing in reach",
		ItemKind::Number, 0.5f, 30.0f, 0.5f, 1, false,
		"Render", "CrosshairDistanceMetres",
		+[](const Config& c) { return c.tracker.crosshairDistanceMetres; },
		+[](Config& c, float v) { c.tracker.crosshairDistanceMetres = v; },
	},
	{
		"Aiming", "Crosshair easing", "How fast the depth follows. Lower is calmer",
		ItemKind::Number, 1.0f, 30.0f, 1.0f, 0, false,
		"Render", "CrosshairDepthSpeed",
		+[](const Config& c) { return c.tracker.crosshairDepthSpeed; },
		+[](Config& c, float v) { c.tracker.crosshairDepthSpeed = v; },
	},
	{
		"Aiming", "Crosshair size", "Metres wide at one metre away",
		ItemKind::Number, 0.005f, 0.15f, 0.005f, 3, false,
		"Render", "CrosshairSizeAtOneMetre",
		+[](const Config& c) { return c.tracker.crosshairSizeAtOneMetre; },
		+[](Config& c, float v) { c.tracker.crosshairSizeAtOneMetre = v; },
	},
	{
		"Aiming", "Crosshair only when needed", "Hide it until you aim or can act",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "CrosshairOnlyWhenNeeded",
		+[](const Config& c) { return c.tracker.crosshairOnlyWhenNeeded ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshairOnlyWhenNeeded = v != 0.0f; },
		"always", "when needed",
	},
	{
		"Aiming", "Crosshair in 3rd person", "Borrows the game's own, which vanilla hides",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "CrosshairInThirdPerson",
		+[](const Config& c) { return c.tracker.crosshairInThirdPerson ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshairInThirdPerson = v != 0.0f; },
	},
	{
		"Aiming", "3rd person only when needed", "Same rule, for the borrowed one",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "CrosshairOnlyWhenNeeded3rdPerson",
		+[](const Config& c) {
			return c.tracker.crosshairOnlyWhenNeededThirdPerson ? 1.0f : 0.0f;
		},
		+[](Config& c, float v) { c.tracker.crosshairOnlyWhenNeededThirdPerson = v != 0.0f; },
		"always", "when needed",
	},
	{
		"Aiming", "Crosshair cutout", "Percent of screen lifted. Raise if bits are left",
		ItemKind::Number, 1.0f, 10.0f, 0.5f, 1, false,
		"Render", "CrosshairSourceShare",
		+[](const Config& c) { return c.tracker.crosshairSourceShare; },
		+[](Config& c, float v) { c.tracker.crosshairSourceShare = v; },
	},

	// ---- Dialogue ----------------------------------------------------------
	{
		"Dialogue", "Zoom on talking", "Vanilla's zoom into a face. Off in VR by default",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DialogZoom",
		+[](const Config& c) { return c.dialogZoom ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.dialogZoom = v != 0.0f; },
	},
	{
		"Dialogue", "First person to talk", "Vanilla flips to first person for a conversation",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DialogFirstPerson",
		+[](const Config& c) { return c.dialogFirstPerson ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.dialogFirstPerson = v != 0.0f; },
	},

	// ---- Screen and menus --------------------------------------------------
	{
		"Screen", "Show menus", "Whether OBVR shows the game's menus at all",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "ShowMenus",
		+[](const Config& c) { return c.tracker.showMenus ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.showMenus = v != 0.0f; },
	},
	{
		"Screen", "HUD overlay", "The flat picture as an overlay quad",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "HudOverlay",
		+[](const Config& c) { return c.tracker.hudOverlay ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.hudOverlay = v != 0.0f; },
	},
	{
		"Screen", "HUD stands in the room", "Off carries it on your head instead",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "HudAnchor",
		+[](const Config& c) { return c.tracker.hudAnchorWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.hudAnchorWorld = v != 0.0f; },
		"head", "world",
	},
	{
		"Screen", "HUD distance", "Metres. How far away the flat picture hangs",
		ItemKind::Number, 0.4f, 8.0f, 0.1f, 1, false,
		"Render", "HudDistanceMetres",
		+[](const Config& c) { return c.tracker.hudDistanceMetres; },
		+[](Config& c, float v) { c.tracker.hudDistanceMetres = v; },
	},
	{
		"Screen", "HUD width", "Metres. How wide it is at that distance",
		ItemKind::Number, 0.4f, 8.0f, 0.1f, 1, false,
		"Render", "HudWidthMetres",
		+[](const Config& c) { return c.tracker.hudWidthMetres; },
		+[](Config& c, float v) { c.tracker.hudWidthMetres = v; },
	},
	{
		"Screen", "Menu scale", "How much of the picture a menu fills",
		ItemKind::Number, 0.3f, 2.0f, 0.05f, 2, false,
		"Render", "MenuScale",
		+[](const Config& c) { return c.tracker.menuScale; },
		+[](Config& c, float v) { c.tracker.menuScale = v; },
	},
	{
		"Screen", "Menus in the world", "Menus hang in front of you instead of on the screen",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		// The one setting Config reads as a word rather than a number. Writing
		// a 1 here leaves a value its own reader rejects - it logs "unknown
		// Render.Menus" and keeps the old setting, so the row would appear to
		// do nothing at all.
		"Render", "Menus",
		+[](const Config& c) { return c.tracker.menusInWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.menusInWorld = v != 0.0f; },
		"cinema", "world",
	},
	{
		"Screen", "Menu shade", "The vanilla brown wash behind a menu",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "MenuShade",
		+[](const Config& c) { return c.tracker.menuShade ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.menuShade = v != 0.0f; },
	},
	{
		"Screen", "Shade strength", "How strong that wash is",
		ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false,
		"Render", "MenuShadeStrength",
		+[](const Config& c) { return c.tracker.menuShadeStrength; },
		+[](Config& c, float v) { c.tracker.menuShadeStrength = v; },
	},
	{
		"Screen", "Field of view", "Degrees. 0 leaves the game's own",
		ItemKind::Number, 0.0f, 140.0f, 1.0f, 0, false,
		"Render", "GameFovOverride",
		+[](const Config& c) { return c.tracker.gameFovOverride; },
		+[](Config& c, float v) { c.tracker.gameFovOverride = v; },
	},
	{
		"Screen", "Base field of view", "Degrees the game believes it is drawing at",
		ItemKind::Number, 30.0f, 140.0f, 1.0f, 0, false,
		"Render", "GameFovDegrees",
		+[](const Config& c) { return c.tracker.gameFovDegrees; },
		+[](Config& c, float v) { c.tracker.gameFovDegrees = v; },
	},
	{
		"Screen", "That FOV is for 4:3", "How the number above is interpreted",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "GameFovIsFor4x3",
		+[](const Config& c) { return c.tracker.gameFovIsFor4x3 ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.gameFovIsFor4x3 = v != 0.0f; },
	},
	{
		"Screen", "Match headset FOV", "Take the field of view from the headset itself",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "MatchHeadsetFov",
		+[](const Config& c) { return c.tracker.matchHeadsetFov ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.matchHeadsetFov = v != 0.0f; },
	},
	{
		"Screen", "Menu aspect", "The shape of the menu picture. 0 leaves it alone",
		ItemKind::Number, 0.0f, 3.0f, 0.05f, 2, false,
		"Render", "MenuAspect",
		+[](const Config& c) { return c.tracker.menuAspect; },
		+[](Config& c, float v) { c.tracker.menuAspect = v; },
	},
	{
		"Screen", "Single menu border", "One border instead of the doubled left and right",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "MenuSingleBorder",
		+[](const Config& c) { return c.tracker.menuSingleBorder ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.menuSingleBorder = v != 0.0f; },
	},
	{
		"Screen", "Menu stand-in", "Holds the world still behind a menu. Also fixes the "
		                           "persuasion face",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "MenuStandIn",
		+[](const Config& c) { return c.tracker.menuStandIn ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.menuStandIn = v != 0.0f; },
	},

	// ---- This menu ---------------------------------------------------------
	//
	// The settings menu's own placement, changeable from inside itself. Worth
	// having in reach: whether a panel hangs at the right distance is a question
	// nobody can answer except while looking at it.
	{
		"This menu", "Stands in the room", "Off carries this panel on your head instead",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"SettingsMenu", "Anchor",
		+[](const Config& c) { return c.settingsMenuInWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.settingsMenuInWorld = v != 0.0f; },
		"head", "world",
	},
	{
		"This menu", "Distance", "Metres in front of you",
		ItemKind::Number, 0.3f, 4.0f, 0.05f, 2, false,
		"SettingsMenu", "DistanceMetres",
		+[](const Config& c) { return c.settingsMenuDistanceMetres; },
		+[](Config& c, float v) { c.settingsMenuDistanceMetres = v; },
	},
	{
		"This menu", "Width", "Metres wide at that distance",
		ItemKind::Number, 0.3f, 4.0f, 0.05f, 2, false,
		"SettingsMenu", "WidthMetres",
		+[](const Config& c) { return c.settingsMenuWidthMetres; },
		+[](Config& c, float v) { c.settingsMenuWidthMetres = v; },
	},

	// ---- Advanced ----------------------------------------------------------
	{
		"Advanced", "HUD between passes", "Capture the 2D between the two eye renders",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "HudBetweenPasses",
		+[](const Config& c) { return c.tracker.hudBetweenPasses ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.hudBetweenPasses = v != 0.0f; },
	},
	{
		"Advanced", "Submit at frame end", "Hand the eyes over at Present instead",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "SubmitAtFrameEnd",
		+[](const Config& c) { return c.tracker.submitAtFrameEnd ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.submitAtFrameEnd = v != 0.0f; },
	},
	{
		"Advanced", "UI follows frame size", "Lay the 2D out at the real frame size",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "UiFollowsFrameSize",
		+[](const Config& c) { return c.tracker.uiFollowsFrameSize ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.uiFollowsFrameSize = v != 0.0f; },
	},
	{
		"Advanced", "Re-read the INI", "Frames between reloads of this file. 0 is off",
		ItemKind::Number, 0.0f, 600.0f, 30.0f, 0, false,
		"Debug", "ReloadEveryFrames",
		+[](const Config& c) { return static_cast<float>(c.reloadEveryFrames); },
		+[](Config& c, float v) { c.reloadEveryFrames = static_cast<UInt32>(v); },
	},

	// ---- Needs a restart ---------------------------------------------------
	//
	// Kept last and marked. These are read while the device is being created,
	// so changing one here has no effect until the game starts again - and
	// somebody who changes a setting and sees nothing happen concludes the menu
	// does not work, not that this particular row is different.
	{
		"Restart needed", "Set the frame size", "Off leaves the game its own resolution",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, true,
		"Render", "SetGameResolution",
		+[](const Config& c) { return c.tracker.setRenderSize ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.setRenderSize = v != 0.0f; },
	},
	{
		"Restart needed", "Render width", "Pixels per eye. 0 asks the headset",
		ItemKind::Number, 0.0f, 8192.0f, 64.0f, 0, true,
		"Render", "GameResolutionWidth",
		+[](const Config& c) { return static_cast<float>(c.tracker.renderWidth); },
		+[](Config& c, float v) { c.tracker.renderWidth = static_cast<UInt32>(v); },
	},
	{
		"Restart needed", "Render height", "Pixels per eye. 0 asks the headset",
		ItemKind::Number, 0.0f, 8192.0f, 64.0f, 0, true,
		"Render", "GameResolutionHeight",
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
