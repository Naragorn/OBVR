#include "ui/SettingsList.h"

namespace obvr::ui {

namespace {

// Captureless lambdas so that each row's reach into the configuration sits on
// the row itself. Naming a member twice - once to read, once to write - is what
// makes the "no row disturbs another" test able to find a mistake at all: there
// is nothing else to compare against.
const SettingDefinition kSettings[] = {
	// ---- Actions -----------------------------------------------------------
	//
	// A button, not a setting: it has no value and no INI home, and firing it
	// is the caller's business (SettingsMenu::Apply hands the row back). The
	// reader and writer exist so the sweeps over the table stay meaningful.
	{
		"Actions", "Recenter view", "Take where the head is now as forward and level",
		ItemKind::Action, 0.0f, 0.0f, 0.0f, 0, false,
		"", "",
		+[](const Config&) { return 0.0f; },
		+[](Config&, float) {},
		"", "", SettingAction::Recenter,
	},
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

	// ---- Aiming ------------------------------------------------------------
	{
		"Aiming", "Aim follows gaze", "Shots leave where you are looking",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "AimFollowsGaze",
		+[](const Config& c) { return c.aimFollowsGaze ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.aimFollowsGaze = v != 0.0f; },
	},
	{
		"Aiming", "Aim in third person", "The same aiming with the chase camera",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "AimInThirdPerson",
		+[](const Config& c) { return c.aimInThirdPerson ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.aimInThirdPerson = v != 0.0f; },
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
	},
	{
		"Aiming", "Crosshair distance", "Metres. Where it hangs with nothing in reach",
		ItemKind::Number, 0.5f, 30.0f, 0.5f, 1, false,
		"Render", "CrosshairDistanceMetres",
		+[](const Config& c) { return c.tracker.crosshairDistanceMetres; },
		+[](Config& c, float v) { c.tracker.crosshairDistanceMetres = v; },
	},
	{
		"Aiming", "Crosshair easing", "How fast depth follows. 0 moves instantly",
		ItemKind::Number, 0.0f, 30.0f, 1.0f, 0, false,
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
	},
	{
		"Aiming", "Crosshair in 3rd person", "Borrows the game's own, which vanilla hides",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "CrosshairInThirdPerson",
		+[](const Config& c) { return c.tracker.crosshairInThirdPerson ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshairInThirdPerson = v != 0.0f; },
	},
	{
		"Aiming", "Tooltips above target name", "Move action icons to the lower-right HUD",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "CrosshairTooltipsAboveName",
		+[](const Config& c) { return c.tracker.crosshairTooltipsAboveName ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshairTooltipsAboveName = v != 0.0f; },
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
		"Screen", "Live 3D pause world", "Redraw the paused Esc/Tab world from the HMD pose",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "LiveMenuBackground",
		+[](const Config& c) { return c.tracker.liveMenuBackground ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.liveMenuBackground = v != 0.0f; },
	},
	{
		"Screen", "Unpaused menus", "The world keeps running behind inventory, map, magic, stats",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "UnpausedMenus",
		+[](const Config& c) { return c.tracker.unpausedMenus ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.unpausedMenus = v != 0.0f; },
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

	// ---- Needs a restart ---------------------------------------------------
	//
	// Kept last and marked. These are read while the device is being created,
	// so changing one here has no effect until the game starts again - and
	// somebody who changes a setting and sees nothing happen concludes the menu
	// does not work, not that this particular row is different.
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
	{
		"Hands", "Hand tracking", "The weapon hand follows the right controller",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "Enabled",
		+[](const Config& c) { return c.handTracking ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.handTracking = v != 0.0f; c.hands.enabled = v != 0.0f; },
	},
	{
		"Hands", "HUD on the wrist", "The HUD hangs on the right wrist",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "WristHud",
		+[](const Config& c) { return c.hands.wristHud ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.wristHud = v != 0.0f; },
	},
	{
		"Hands", "Menus on the wrist", "The Tab menus hang on a wrist",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "WristMenu",
		+[](const Config& c) { return c.hands.wristMenu ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.wristMenu = v != 0.0f; },
	},
	{
		"Hands", "Hide the arms", "The animated arms go; hands and weapon stay",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "HideArms",
		+[](const Config& c) { return c.hands.hideArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.hideArms = v != 0.0f; },
	},
	{
		"Hands", "Hands on the controllers", "The hand bones follow the controllers, not the animation",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "PinHands",
		+[](const Config& c) { return c.hands.pinHands ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.pinHands = v != 0.0f; },
	},
	{
		"Hands", "Right hand roll", "Turns the right palm about the bone, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "RightHandRoll",
		+[](const Config& c) { return c.hands.rightHandRoll; },
		+[](Config& c, float v) { c.hands.rightHandRoll = v; },
	},
	{
		"Hands", "Right hand yaw", "Turns the right hand about the up axis, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "RightHandYaw",
		+[](const Config& c) { return c.hands.rightHandYaw; },
		+[](Config& c, float v) { c.hands.rightHandYaw = v; },
	},
	{
		"Hands", "Left hand roll", "Turns the left palm about the bone, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "LeftHandRoll",
		+[](const Config& c) { return c.hands.leftHandRoll; },
		+[](Config& c, float v) { c.hands.leftHandRoll = v; },
	},
	{
		"Hands", "Left hand yaw", "Turns the left hand about the up axis, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "LeftHandYaw",
		+[](const Config& c) { return c.hands.leftHandYaw; },
		+[](Config& c, float v) { c.hands.leftHandYaw = v; },
	},
	{
		"Hands", "Force first person", "Back to first person whenever the mode finds third",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ForceFirstPerson",
		+[](const Config& c) { return c.hands.forceFirstPerson ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.forceFirstPerson = v != 0.0f; },
	},
	{
		"Hands", "Menus on the right hand", "On: right wrist, the left hand presses. Off: the other way",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "MenuOnRight",
		+[](const Config& c) { return c.hands.menuOnRight ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.menuOnRight = v != 0.0f; },
	},
	{
		"Hands", "HUD size on the wrist", "Width of the wrist HUD in metres",
		ItemKind::Number, 0.10f, 1.00f, 0.05f, 2, false,
		"Hands", "WristHudWidth",
		+[](const Config& c) { return c.hands.wristHudWidth; },
		+[](Config& c, float v) { c.hands.wristHudWidth = v; },
	},
	{
		"Hands", "Menu size on the wrist", "Width of the wrist menu in metres",
		ItemKind::Number, 0.20f, 1.50f, 0.05f, 2, false,
		"Hands", "WristMenuWidth",
		+[](const Config& c) { return c.hands.wristMenuWidth; },
		+[](Config& c, float v) { c.hands.wristMenuWidth = v; },
	},
	{
		"Hands", "Swing speed", "Metres per second that count as a swing",
		ItemKind::Number, 0.5f, 5.0f, 0.1f, 1, false,
		"Hands", "SwingLight",
		+[](const Config& c) { return c.hands.gestures.swingLight; },
		+[](Config& c, float v) { c.hands.gestures.swingLight = v; },
	},
	{
		"Hands", "Heavy swing speed", "Metres per second that make a swing a power attack",
		ItemKind::Number, 1.0f, 8.0f, 0.1f, 1, false,
		"Hands", "SwingHeavy",
		+[](const Config& c) { return c.hands.gestures.swingHeavy; },
		+[](Config& c, float v) { c.hands.gestures.swingHeavy = v; },
	},
	{
		"Hands", "Strikes by motion", "The swung blade hits what it passes through, no animation",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "MotionHits",
		+[](const Config& c) { return c.hands.motionHits ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.motionHits = v != 0.0f; },
	},
	{
		"Hands", "Strike size", "How much of a body's bound the blade has to reach, 0 to 1",
		ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false,
		"Hands", "HitBoundFactor",
		+[](const Config& c) { return c.hands.hitBoundFactor; },
		+[](Config& c, float v) { c.hands.hitBoundFactor = v; },
	},
	{
		"Help", "Introduction at start", "Show the first-start walkthrough again next time",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Onboarding", "ShowAtStart",
		+[](const Config& c) { return c.onboardingShowAtStart ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.onboardingShowAtStart = v != 0.0f; },
	},
};

// Two short strings, compared the only way a freestanding build can.
bool SameKey(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return false;
	}
	while (*a != '\0' && *a == *b) {
		++a;
		++b;
	}
	return *a == *b;
}

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

const SettingDefinition* FindSetting(const char* iniSection, const char* iniKey) {
	for (const SettingDefinition& definition : kSettings) {
		if (SameKey(definition.iniSection, iniSection) && SameKey(definition.iniKey, iniKey)) {
			return &definition;
		}
	}
	return nullptr;
}

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
