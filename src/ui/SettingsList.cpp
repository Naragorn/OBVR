#include "ui/SettingsList.h"

namespace obvr::ui {

namespace {

// The HUD on the hands has the same three rows for every element; one reader
// and writer per row, told the element by the template argument.
template <UInt32 E>
float HudPlaceRead(const Config& c) {
	return static_cast<float>(c.hands.handHud.element[E].place);
}
template <UInt32 E>
void HudPlaceWrite(Config& c, float v) {
	c.hands.handHud.element[E].place = vr::HudPlaceFromIndex(v);
}
template <UInt32 E>
float HudOpacityRead(const Config& c) {
	return c.hands.handHud.element[E].opacity;
}
template <UInt32 E>
void HudOpacityWrite(Config& c, float v) {
	c.hands.handHud.element[E].opacity = v;
}
template <UInt32 E>
float HudSizeRead(const Config& c) {
	return c.hands.handHud.element[E].size;
}
template <UInt32 E>
void HudSizeWrite(Config& c, float v) {
	c.hands.handHud.element[E].size = v;
}

#define OBVR_HUD_ELEMENT_ROWS(E, NAME, KEY)                                                              \
	{                                                                                                    \
		"HUD", NAME, "Where: the panel, left or right hand, the sky, off, top or bottom of the view, under the target",    \
		ItemKind::Number, 0.0f, static_cast<float>(vr::kHudPlaceCount - 1), 1.0f, 0, false,              \
		"HandHud", KEY "Place", &HudPlaceRead<E>, &HudPlaceWrite<E>,                                     \
		"", "", SettingAction::None, vr::kHudPlaceNames, vr::kHudPlaceCount,                             \
	},                                                                                                   \
	{                                                                                                    \
		"HUD", NAME " opacity", "How solid it is, 0 to 1", ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false, \
		"HandHud", KEY "Opacity", &HudOpacityRead<E>, &HudOpacityWrite<E>,                               \
	},                                                                                                   \
	{                                                                                                    \
		"HUD", NAME " size", "Times its own size", ItemKind::Number, 0.5f, 3.0f, 0.1f, 1, false,         \
		"HandHud", KEY "Size", &HudSizeRead<E>, &HudSizeWrite<E>,                                        \
	},

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
		"Comfort", "Still death view", "When you die the view stays put instead of sinking",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DeathViewStill",
		+[](const Config& c) { return c.look.deathViewStill ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.deathViewStill = v != 0.0f; },
	},
	{
		"Comfort", "Grey on death", "The picture turns grey when you die",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DeathGrey",
		+[](const Config& c) { return c.look.deathGrey ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.deathGrey = v != 0.0f; },
	},
	{
		"Comfort", "Death tint: menu colour", "Tint in the menus' brown when you die, not grey",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DeathMenuTint",
		+[](const Config& c) { return c.look.deathMenuTint ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.deathMenuTint = v != 0.0f; },
	},
	{
		"Comfort", "Body view before load", "At the end of a death the view goes to your body",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DeathBodyView",
		+[](const Config& c) { return c.look.deathBodyView ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.deathBodyView = v != 0.0f; },
	},
	{
		"Comfort", "Death view step back", "How far behind you the still death view stands, metres",
		ItemKind::Number, 0.0f, 2.0f, 0.1f, 1, false,
		"Look", "DeathViewBackMetres",
		+[](const Config& c) { return c.look.deathViewBackMetres; },
		+[](Config& c, float v) { c.look.deathViewBackMetres = v; },
	},
	{
		"Comfort", "Death body ahead", "How far in front of you your body falls when you die, metres",
		ItemKind::Number, 0.0f, 2.0f, 0.1f, 1, false,
		"Look", "DeathBodyAheadMetres",
		+[](const Config& c) { return c.look.deathBodyAheadMetres; },
		+[](Config& c, float v) { c.look.deathBodyAheadMetres = v; },
	},
	{
		"Comfort", "Death body height", "How far above (negative: below) where it falls your body is shown, metres",
		ItemKind::Number, -1.0f, 1.0f, 0.05f, 2, false,
		"Look", "DeathBodyUpMetres",
		+[](const Config& c) { return c.look.deathBodyUpMetres; },
		+[](Config& c, float v) { c.look.deathBodyUpMetres = v; },
	},
	{
		"Comfort", "Hide HUD when dead", "The HUD and crosshair go when you die",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "HideHudWhenDead",
		+[](const Config& c) { return c.look.hideHudWhenDead ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.hideHudWhenDead = v != 0.0f; },
	},
	{
		"Comfort", "Fades in view", "The game's fades darken your whole view, not a panel",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "FadesInView",
		+[](const Config& c) { return c.look.fadesInView ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.fadesInView = v != 0.0f; },
	},
	{
		"Comfort", "No stagger", "Hits never stagger you or knock you back",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "NoPlayerStagger",
		+[](const Config& c) { return c.look.noPlayerStagger ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.noPlayerStagger = v != 0.0f; },
	},
	{
		"Comfort", "No knockdown", "Blows, explosions and spells never throw you through the air",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "NoPlayerKnockdown",
		+[](const Config& c) { return c.look.noPlayerKnockdown ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.noPlayerKnockdown = v != 0.0f; },
	},
	{
		"Comfort", "Exhaustion, not collapse",
		"Out of fatigue, or paralysed, you stay on your feet: your weapon turns heavy and you breathe hard instead "
		"of falling to the ground",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "Exhaustion",
		+[](const Config& c) { return c.look.exhaustion.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.exhaustion.enabled = v != 0.0f; },
	},
	{
		"Comfort", "Exhausted below (%)", "Fatigue under this share of your maximum exhausts you",
		ItemKind::Number, 1.0f, 50.0f, 1.0f, 0, false,
		"Look", "ExhaustionBelowPercent",
		+[](const Config& c) { return c.look.exhaustion.belowPercent; },
		+[](Config& c, float v) { c.look.exhaustion.belowPercent = v; },
	},
	{
		"Comfort", "Recovered above (%)", "Fatigue back over this share of your maximum ends the exhaustion",
		ItemKind::Number, 1.0f, 90.0f, 1.0f, 0, false,
		"Look", "ExhaustionAbovePercent",
		+[](const Config& c) { return c.look.exhaustion.abovePercent; },
		+[](Config& c, float v) { c.look.exhaustion.abovePercent = v; },
	},
	{
		"Comfort", "Exhausted weapon weight (x)", "How many times heavier the weapon trails your hand while exhausted",
		ItemKind::Number, 1.0f, 10.0f, 0.5f, 1, false,
		"Look", "ExhaustionWeight",
		+[](const Config& c) { return c.look.exhaustion.weight; },
		+[](Config& c, float v) { c.look.exhaustion.weight = v; },
	},
	{
		"Comfort", "Exhaustion sounds",
		"The game's own gasps while exhausted: one when a fall was spared, then panting every few heartbeats",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "ExhaustionSounds",
		+[](const Config& c) { return c.look.exhaustion.sounds ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.exhaustion.sounds = v != 0.0f; },
	},
	{
		"Comfort", "No hit blur", "Hits never blur or shake your view",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "NoHitBlur",
		+[](const Config& c) { return c.look.noHitBlur ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.noHitBlur = v != 0.0f; },
	},
	{
		"Comfort", "Idle camera orbit", "The game's third-person orbit after a while without input; off in the headset",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "AutoVanityCamera",
		+[](const Config& c) { return c.look.autoVanityCamera ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.autoVanityCamera = v != 0.0f; },
	},
	{
		"Comfort", "Horse bob (0-1)", "How much of the horse's walk and gallop moves your view; 0 a steady saddle",
		ItemKind::Number, 0.0f, 1.0f, 0.1f, 1, false,
		"Look", "HorseBob",
		+[](const Config& c) { return c.look.horseBob; },
		+[](Config& c, float v) { c.look.horseBob = v; },
	},
	{
		"Comfort", "Race menu before your face", "The race menu's view stands in front of your character, facing it, as the game's own; off: the view stays where it was",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "ChargenView",
		+[](const Config& c) { return c.look.chargenView ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.chargenView = v != 0.0f; },
	},
	{
		"Comfort", "Race menu distance (m)", "How far in front of the face the race menu's view stands; the game's own is 0.43",
		ItemKind::Number, 0.4f, 3.0f, 0.1f, 1, false,
		"Look", "ChargenDistanceMetres",
		+[](const Config& c) { return c.look.chargenDistanceMetres; },
		+[](Config& c, float v) { c.look.chargenDistanceMetres = v; },
	},
	{
		"Comfort", "Block where you look", "Your block counts towards where you look, not where your body faces",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "BlockFacesView",
		+[](const Config& c) { return c.look.blockFacesView ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.blockFacesView = v != 0.0f; },
	},
	{
		"Comfort", "Eye separation", "Wider feels smaller, narrower feels larger",
		ItemKind::Number, 0.7f, 1.3f, 0.01f, 2, false,
		"Render", "EyeSeparationScale",
		+[](const Config& c) { return c.tracker.eyeSeparationScale; },
		+[](Config& c, float v) { c.tracker.eyeSeparationScale = v; },
	},
	{
		"Comfort", "Snap turning", "Right stick left/right snaps instead of continuous turn",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "SnapTurning",
		+[](const Config& c) { return c.look.snapTurning ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.snapTurning = v != 0.0f; },
	},
	{
		"Comfort", "Snap angle", "Degrees per snap turn",
		ItemKind::Number, 15.0f, 90.0f, 5.0f, 0, false,
		"Look", "SnapTurnAngle",
		+[](const Config& c) { return c.look.snapTurnAngle; },
		+[](Config& c, float v) { c.look.snapTurnAngle = v; },
	},
	{
		"Comfort", "Instant snap", "Snaps instantly instead of easing into place",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "SnapTurnInstant",
		+[](const Config& c) { return c.look.snapTurnInstant ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.snapTurnInstant = v != 0.0f; },
	},
	{
		"Comfort", "Snap speed", "How fast an eased snap catches up",
		ItemKind::Number, 5.0f, 40.0f, 1.0f, 0, false,
		"Look", "SnapTurnSpeed",
		+[](const Config& c) { return c.look.snapTurnSpeed; },
		+[](Config& c, float v) { c.look.snapTurnSpeed = v; },
	},
	{
		"Comfort", "Snap vignette", "Darkens screen edges when a snap turn fires",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "SnapTurnVignette",
		+[](const Config& c) { return c.look.snapTurnVignette ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.snapTurnVignette = v != 0.0f; },
	},
	{
		"Comfort", "Vignette radius", "Degrees of clear view from the centre before it darkens",
		ItemKind::Number, 5.0f, 30.0f, 1.0f, 0, false,
		"Look", "SnapTurnVignetteRadius",
		+[](const Config& c) { return c.look.snapTurnVignetteRadius; },
		+[](Config& c, float v) { c.look.snapTurnVignetteRadius = v; },
	},
	{
		"Comfort", "Vignette darkness", "How dark the vignette gets at the edge, 0 to 1",
		ItemKind::Number, 0.1f, 1.0f, 0.05f, 2, false,
		"Look", "SnapTurnVignetteStrength",
		+[](const Config& c) { return c.look.snapTurnVignetteStrength; },
		+[](Config& c, float v) { c.look.snapTurnVignetteStrength = v; },
	},
	{
		"Comfort", "Snap dead zone", "How far to push the stick before a snap fires",
		ItemKind::Number, 0.15f, 0.7f, 0.05f, 2, false,
		"Look", "SnapTurnDeadZone",
		+[](const Config& c) { return c.look.snapTurnDeadZone; },
		+[](Config& c, float v) { c.look.snapTurnDeadZone = v; },
	},
	{
		"Comfort", "Ease the turn", "The camera glides after a mouse or gamepad turn",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "SmoothTurning",
		+[](const Config& c) { return c.look.smoothTurning ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.smoothTurning = v != 0.0f; },
	},
	{
		"Comfort", "Ease speed", "How fast an eased turn catches up",
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
		"Looking", "Headset steers the view", "Off: the mouse and the stick turn and tilt the view, the head does nothing",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Head", "HeadLook",
		+[](const Config& c) { return c.tracker.headLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.headLook = v != 0.0f; },
	},
	{
		"Looking", "Head steers the vertical look", "Up and down from the head, not the mouse; forced on with Aim at source, off without headset look",
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
	{
		"Dialogue", "Panel on the speaker", "When a conversation opens, the panel moves to who you talk to",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DialogRecentre",
		+[](const Config& c) { return c.dialogPanel.recentre ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.dialogPanel.recentre = v != 0.0f; },
	},
	{
		"Dialogue", "Panel side", "In front of the speaker, or beside them so they stay in view",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kDialogPanelSideCount - 1), 1.0f, 0, false,
		"Look", "DialogPanelSide",
		+[](const Config& c) { return static_cast<float>(c.dialogPanel.side); },
		+[](Config& c, float v) { c.dialogPanel.side = vr::DialogPanelSideFromIndex(v); },
		"", "", SettingAction::None, vr::kDialogPanelSideNames, vr::kDialogPanelSideCount,
	},
	{
		"Dialogue", "Panel beside by", "How far beside the speaker, degrees",
		ItemKind::Number, 5.0f, 60.0f, 5.0f, 0, false,
		"Look", "DialogPanelSideDegrees",
		+[](const Config& c) { return c.dialogPanel.sideDegrees; },
		+[](Config& c, float v) { c.dialogPanel.sideDegrees = v; },
	},
	{
		"Dialogue", "Panel size", "The dialogue panel's size, times the menus'",
		ItemKind::Number, 0.4f, 1.5f, 0.05f, 2, false,
		"Look", "DialogPanelScale",
		+[](const Config& c) { return c.dialogPanel.scale; },
		+[](Config& c, float v) { c.dialogPanel.scale = v; },
	},

	// ---- Body --------------------------------------------------------------
	{
		"Body", "Your own body", "Look down and see a body under the headset",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "Visible",
		+[](const Config& c) { return c.body.visible ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.visible = v != 0.0f; },
	},
	{
		"Body", "Follows you (IK)", "The body's arms reach your hands, it turns with you, it crouches with you",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IK",
		+[](const Config& c) { return c.body.ik ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ik = v != 0.0f; },
	},
	{
		"Body", "Arms reach your hands", "Each arm of the body is bent to meet the hand you hold out",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKArms",
		+[](const Config& c) { return c.body.ikArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikArms = v != 0.0f; },
	},
	{
		"Body", "Turns with your head", "The body stands steady under your head and follows where you look: a glance leaves it, a held look brings it round",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKTorso",
		+[](const Config& c) { return c.body.ikTorso ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikTorso = v != 0.0f; },
	},
	{
		"Body", "Knees bend when you crouch", "Crouch for real and the body's knees bend, its feet stay on the ground",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKLegs",
		+[](const Config& c) { return c.body.ikLegs ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikLegs = v != 0.0f; },
	},
	{
		"Body", "Head's free turn (deg)", "How far you can look aside before the body turns with you",
		ItemKind::Number, 0.0f, 90.0f, 5.0f, 0, false,
		"Body", "TorsoFollowDegrees",
		+[](const Config& c) { return c.body.torsoFollowDegrees; },
		+[](Config& c, float v) { c.body.torsoFollowDegrees = v; },
	},
	{
		"Body", "Body follows after (s)", "How long a held look takes to bring the body round",
		ItemKind::Number, 0.0f, 5.0f, 0.1f, 1, false,
		"Body", "TorsoFollowSeconds",
		+[](const Config& c) { return c.body.torsoFollowSeconds; },
		+[](Config& c, float v) { c.body.torsoFollowSeconds = v; },
	},
	{
		"Body", "Arm stretch", "How much longer the body's arms may grow to reach your hands",
		ItemKind::Number, 1.0f, 1.5f, 0.05f, 2, false,
		"Body", "ArmStretch",
		+[](const Config& c) { return c.body.armStretch; },
		+[](Config& c, float v) { c.body.armStretch = v; },
	},
	{
		"Body", "Hide its arms", "Arms that do not follow your hands go; the first-person arms stay",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "HideArms",
		+[](const Config& c) { return c.body.hideArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.hideArms = v != 0.0f; },
	},
	{
		"Body", "Eyes forward", "How far the eyes sit in front of the head bone, in units",
		ItemKind::Number, -20.0f, 40.0f, 0.2f, 1, false,
		"Body", "EyeForward",
		+[](const Config& c) { return c.body.eyeForward; },
		+[](Config& c, float v) { c.body.eyeForward = v; },
	},
	{
		"Body", "Eyes up", "How far the eyes sit above the head bone, in units",
		ItemKind::Number, -20.0f, 30.0f, 1.0f, 0, false,
		"Body", "EyeUp",
		+[](const Config& c) { return c.body.eyeUp; },
		+[](Config& c, float v) { c.body.eyeUp = v; },
	},
	{
		"Body", "Near plane", "Units from the eyes where the picture begins; nearer shows more chest looking down, 0 is the game's 10",
		ItemKind::Number, 0.0f, 10.0f, 0.5f, 1, false,
		"Body", "NearPlane",
		+[](const Config& c) { return c.body.nearPlaneUnits; },
		+[](Config& c, float v) { c.body.nearPlaneUnits = v; },
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
		"Screen", "Container over the chest", "A container's menu opens over the container, the world running",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "ContainerInWorld",
		+[](const Config& c) { return c.containerPanel.inWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.containerPanel.inWorld = v != 0.0f; },
	},
	{
		"Screen", "Container panel size", "The container panel's size, times the menus'",
		ItemKind::Number, 0.3f, 1.5f, 0.05f, 2, false,
		"Look", "ContainerPanelScale",
		+[](const Config& c) { return c.containerPanel.scale; },
		+[](Config& c, float v) { c.containerPanel.scale = v; },
	},
	{
		"Screen", "Container panel height", "How far (m) the panel's middle sits over the container's middle",
		ItemKind::Number, -0.5f, 1.0f, 0.05f, 2, false,
		"Look", "ContainerPanelRaiseMetres",
		+[](const Config& c) { return c.containerPanel.raiseMetres; },
		+[](Config& c, float v) { c.containerPanel.raiseMetres = v; },
	},
	{
		"Screen", "Flat picture depth (m)",
		"How far away the main menu, loading screens and the cinema screen are shown; 0 is infinity. Takes effect as "
		"the eye copies are remade",
		ItemKind::Number, 0.0f, 10.0f, 0.5f, 1, false,
		"Render", "FlatDepthMetres",
		+[](const Config& c) { return c.tracker.flatDepthMetres; },
		+[](Config& c, float v) { c.tracker.flatDepthMetres = v; },
	},
	{
		"Screen", "Menu shade", "The vanilla brown wash behind a menu",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "MenuShade",
		+[](const Config& c) { return c.tracker.menuShade ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.menuShade = v != 0.0f; },
	},
	{
		"Screen", "Black loading screens", "Black behind the loading art, not the frozen world and hands",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "LoadingBlack",
		+[](const Config& c) { return c.tracker.loadingBlack ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.loadingBlack = v != 0.0f; },
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
		"Mode", "Full VR mode", "On: the Full VR Port - hands, gestures, teleport. Off: VR View",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "Enabled",
		+[](const Config& c) { return c.fullVrMode ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.fullVrMode = v != 0.0f; c.hands.enabled = v != 0.0f; },
	},
	// ---- Hands
	{
		"Hands", "Hands on the controllers", "The hand bones follow the controllers, not the animation",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "PinHands",
		+[](const Config& c) { return c.hands.pinHands ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.pinHands = v != 0.0f; },
	},
	{
		"Hands", "Finger tracking", "An empty hand's fingers follow yours on the controller",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "FingerTracking",
		+[](const Config& c) { return c.hands.fingerTracking ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.fingerTracking = v != 0.0f; },
	},
	{
		"Hands", "Hide the arms", "The animated arms go; hands and weapon stay",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "HideArms",
		+[](const Config& c) { return c.hands.hideArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.hideArms = v != 0.0f; },
	},
	{
		"Hands", "Closed hands", "The inside of the hands drawn, darker, not see-through at the wrist",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ClosedHands",
		+[](const Config& c) { return c.hands.closedHands ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.closedHands = v != 0.0f; },
	},
	{
		"Hands", "Inside brightness", "How bright the inside of the hands is: 0 black, 1 as the outside",
		ItemKind::Number, 0.0f, 1.0f, 0.1f, 1, false,
		"Hands", "ClosedHandsBrightness",
		+[](const Config& c) { return c.hands.closedHandsBrightness; },
		+[](Config& c, float v) { c.hands.closedHandsBrightness = v; },
	},
	{
		"Hands", "Close bare wrists", "A bare hand's wrist drawn closed, not open to look into",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "CloseBareWrists",
		+[](const Config& c) { return c.hands.closeBareWrists ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.closeBareWrists = v != 0.0f; },
	},
	{
		"Hands", "Hand height", "Moves both hands along the controller's up axis, metres; lower is negative",
		ItemKind::Number, -0.15f, 0.10f, 0.01f, 2, false,
		"Hands", "HandGripUpMetres",
		+[](const Config& c) { return c.hands.handGripUpMetres; },
		+[](Config& c, float v) { c.hands.handGripUpMetres = v; },
	},
	{
		"Hands", "Hand forward", "Moves both hands along the controller's forward axis, metres",
		ItemKind::Number, -0.15f, 0.15f, 0.01f, 2, false,
		"Hands", "HandGripForwardMetres",
		+[](const Config& c) { return c.hands.handGripForwardMetres; },
		+[](Config& c, float v) { c.hands.handGripForwardMetres = v; },
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
		"Hands", "Right hand pitch", "Tips the right hand up or down about its side axis, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "RightHandPitch",
		+[](const Config& c) { return c.hands.rightHandPitch; },
		+[](Config& c, float v) { c.hands.rightHandPitch = v; },
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
		"Hands", "Left hand pitch", "Tips the left hand up or down about its side axis, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "LeftHandPitch",
		+[](const Config& c) { return c.hands.leftHandPitch; },
		+[](Config& c, float v) { c.hands.leftHandPitch = v; },
	},
	{
		"Hands", "Adjust hands", "A guided fit of the in-game hands to the controllers you hold",
		ItemKind::Action, 0.0f, 0.0f, 0.0f, 0, false,
		"", "",
		+[](const Config&) { return 0.0f; },
		+[](Config&, float) {},
		"", "", SettingAction::AdjustHands,
	},
	{
		"Hands", "Left-handed", "The controllers swap roles: weapon, attack and swing on the left",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "LeftHanded",
		+[](const Config& c) { return c.hands.leftHanded ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.leftHanded = v != 0.0f; },
	},
	{
		"Hands", "Controllers in menus", "With the mode off: laser at the menus, sticks in this one",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ControllerMenus",
		+[](const Config& c) { return c.hands.controllerMenus ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.controllerMenus = v != 0.0f; },
	},
	{
		"Hands", "Controllers as a gamepad", "With the mode off: the pad layout in the world, the head aims",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "GamepadLayout",
		+[](const Config& c) { return c.hands.gamepadLayout ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.gamepadLayout = v != 0.0f; },
	},
	// ---- Hands: grabbing
	{
		"Hands: grabbing", "Grab reach", "How close the hand has to be for the grip to pick a thing up, metres",
		ItemKind::Number, 0.1f, 0.8f, 0.05f, 2, false,
		"Hands", "GrabReachMetres",
		+[](const Config& c) { return c.hands.grabReachMetres; },
		+[](Config& c, float v) { c.hands.grabReachMetres = v; },
	},
	{
		"Hands: grabbing", "Pull reach", "How far a grip pulls an item it points at into the hand, metres; 0 is off",
		ItemKind::Number, 0.0f, 3.0f, 0.1f, 1, false,
		"Hands", "PullReachMetres",
		+[](const Config& c) { return c.hands.pullReachMetres; },
		+[](Config& c, float v) { c.hands.pullReachMetres = v; },
	},
	{
		"Hands: grabbing", "Pull speed", "How fast an item floats into the hand, metres a second",
		ItemKind::Number, 0.5f, 10.0f, 0.5f, 1, false,
		"Hands", "PullSpeedMetres",
		+[](const Config& c) { return c.hands.pullSpeedMetres; },
		+[](Config& c, float v) { c.hands.pullSpeedMetres = v; },
	},
	{
		"Hands: grabbing", "Held object distance", "How far from the palm along the fingers a held object sits, metres",
		ItemKind::Number, -0.2f, 0.4f, 0.02f, 2, false,
		"Hands", "HeldObjectMetres",
		+[](const Config& c) { return c.hands.heldObjectMetres; },
		+[](Config& c, float v) { c.hands.heldObjectMetres = v; },
	},
	{
		"Hands: grabbing", "Grip curl", "How far the fingers close around a held object; negative flips",
		ItemKind::Number, -90.0f, 90.0f, 5.0f, 0, false,
		"Hands", "GripCurlDegrees",
		+[](const Config& c) { return c.hands.gripCurlDegrees; },
		+[](Config& c, float v) { c.hands.gripCurlDegrees = v; },
	},
	{
		"Hands: grabbing", "Levitate objects", "Held things float on the game's spring instead of sitting in the hand",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "LevitateObjects",
		+[](const Config& c) { return c.hands.levitateObjects ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.levitateObjects = v != 0.0f; },
	},
	{
		"Hands: grabbing", "Small objects in the hand", "Potions, keys and the like sit fixed in the palm",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "AttachSmallObjects",
		+[](const Config& c) { return c.hands.attachSmallObjects ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.attachSmallObjects = v != 0.0f; },
	},
	{
		"Hands: grabbing", "Throw strength", "Letting go mid-swing throws with the hand's speed times this",
		ItemKind::Number, 0.0f, 3.0f, 0.1f, 1, false,
		"Hands", "ThrowStrength",
		+[](const Config& c) { return c.hands.throwStrength; },
		+[](Config& c, float v) { c.hands.throwStrength = v; },
	},
	{
		"Hands: grabbing", "Held objects push", "What you hold pushes what it meets, and turns with the hand",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "HeldObjectsPush",
		+[](const Config& c) { return c.hands.heldObjectsPush ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.heldObjectsPush = v != 0.0f; },
	},
	{
		"Hands: grabbing", "Thrown things hit", "What you throw staggers people, or knocks them down when fast",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ThrowHits",
		+[](const Config& c) { return c.hands.throwHit.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.throwHit.enabled = v != 0.0f; },
	},
	{
		"Hands: grabbing", "Throw: stagger from", "How fast (m/s) a thrown thing must fly to stagger someone",
		ItemKind::Number, 1.0f, 15.0f, 0.5f, 1, false,
		"Hands", "ThrowStaggerSpeed",
		+[](const Config& c) { return c.hands.throwHit.staggerSpeed; },
		+[](Config& c, float v) { c.hands.throwHit.staggerSpeed = v; },
	},
	{
		"Hands: grabbing", "Throw: knock down from", "How fast (m/s) it must fly to knock them down",
		ItemKind::Number, 1.0f, 20.0f, 0.5f, 1, false,
		"Hands", "ThrowKnockSpeed",
		+[](const Config& c) { return c.hands.throwHit.knockSpeed; },
		+[](Config& c, float v) { c.hands.throwHit.knockSpeed = v; },
	},
	{
		"Hands: grabbing", "Take only by hand", "Full VR: activating a loose item no longer takes it - stow it",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "TakeOnlyByHand",
		+[](const Config& c) { return c.hands.stow.takeOnlyByHand ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.stow.takeOnlyByHand = v != 0.0f; },
	},
	{
		"Hands: grabbing", "Stow at the body", "Full VR: let go of a held item at your chest or belly to take it",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "StowAtBody",
		+[](const Config& c) { return c.hands.stow.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.stow.enabled = v != 0.0f; },
	},
	{
		"Hands: grabbing", "Place the stow spot", "Shows the ring: grip it and move it where you want it on your body",
		ItemKind::Action, 0.0f, 0.0f, 0.0f, 0, false,
		"", "",
		+[](const Config&) { return 0.0f; },
		+[](Config&, float) {},
		"", "", SettingAction::PlaceStowSpot,
	},
	{
		"Hands: grabbing", "Show the stow ring", "The gold ring shows while you hold an item (the spot stows either way)",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "StowSpotVisible",
		+[](const Config& c) { return c.hands.stow.spotVisible ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.stow.spotVisible = v != 0.0f; },
	},
	// ---- Hands: physics
	{
		"Hands: physics", "Hand collision", "Hands and weapon are physics bodies: they push, tip and hold objects",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "BodyCollision",
		+[](const Config& c) { return c.hands.bodyCollision ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.bodyCollision = v != 0.0f; },
	},
	{
		"Hands: physics", "Physics rate", "Physics steps a second; 0 keeps the game's 60, -10 follows the headset's rate. More costs more",
		ItemKind::Number, -10.0f, 180.0f, 10.0f, 0, false,
		"Hands", "PhysicsRate",
		+[](const Config& c) { return c.hands.physicsRate; },
		+[](Config& c, float v) { c.hands.physicsRate = v; },
	},
	{
		"Hands: physics", "Body radius", "Your own body's width in the physics, times the game's: smaller pushes people and bumps into less",
		ItemKind::Number, 0.3f, 1.5f, 0.05f, 2, false,
		"Hands", "BodyRadiusScale",
		+[](const Config& c) { return c.hands.bodyRadiusScale; },
		+[](Config& c, float v) { c.hands.bodyRadiusScale = v; },
	},
	{
		"Hands: physics", "Push people", "Hands and weapon push the living; off, they pass through them (never in combat or as a fist)",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "PushPeople",
		+[](const Config& c) { return c.hands.pushPeople ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.pushPeople = v != 0.0f; },
	},
	{
		"Hands: physics", "Weapon and hands push", "Your weapon and hands knock and shove the objects they meet",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "PushWorld",
		+[](const Config& c) { return c.hands.pushWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.pushWorld = v != 0.0f; },
	},
	{
		"Hands: physics", "Weapon stops at walls",
		"The drawn weapon stops at walls, floors and furniture and slides along them; pushed too far it lets go",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "WeaponStopsAtWalls",
		+[](const Config& c) { return c.hands.weaponStopsAtWalls ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.weaponStopsAtWalls = v != 0.0f; },
	},
	{
		"Hands: physics", "Weapon lets go at (m)",
		"How far the hand may go past where a wall holds the weapon before it lets go and passes through",
		ItemKind::Number, 0.10f, 1.00f, 0.05f, 2, false,
		"Hands", "WeaponLetGoMetres",
		+[](const Config& c) { return c.hands.weaponLetGoMetres; },
		+[](Config& c, float v) { c.hands.weaponLetGoMetres = v; },
	},
	{
		"Hands: physics", "Lets go at people (m)",
		"How far the hand may go past where a person holds the weapon or the hand before it goes into them; 0 holds for good, as Blade & Sorcery does",
		ItemKind::Number, 0.00f, 1.00f, 0.05f, 2, false,
		"Hands", "BodyLetGoMetres",
		+[](const Config& c) { return c.hands.bodyLetGoMetres; },
		+[](Config& c, float v) { c.hands.bodyLetGoMetres = v; },
	},
	{
		"Hands: physics", "Slices out of people at (degrees)",
		"How far a person may turn your blade off your hand's turn before it slices out of them; 0 never - it slides along them until it comes off",
		ItemKind::Number, 0.0f, 90.0f, 5.0f, 0, false,
		"Hands", "BodyLetGoDegrees",
		+[](const Config& c) { return c.hands.bodyLetGoDegrees; },
		+[](Config& c, float v) { c.hands.bodyLetGoDegrees = v; },
	},
	{
		"Hands: physics", "Weapon rests on people",
		"Held there slowly the weapon rests on a person; a swing is held at them or goes in, by the next setting",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "WeaponStopsAtBodies",
		+[](const Config& c) { return c.hands.weaponStopsAtBodies ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.weaponStopsAtBodies = v != 0.0f; },
	},
	{
		"Hands: physics", "Weapon hit-stop",
		"A swing that goes into someone is slowed for a few frames - the weapon only, never the view",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "WeaponHitStop",
		+[](const Config& c) { return c.hands.weaponHitStop ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.weaponHitStop = v != 0.0f; },
	},
	{
		"Hands: physics", "Swings stop at people",
		"A swing is held at the person it meets, as in Blade & Sorcery; pressed on past the let-go cap it goes in",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "SwingsStopAtBodies",
		+[](const Config& c) { return c.hands.swingsStopAtBodies ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.swingsStopAtBodies = v != 0.0f; },
	},
	{
		"Hands: physics", "Blade slides off people",
		"A person holding your blade turns it; your hand goes on and the blade slides off them, as in Blade & Sorcery",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "BladeSlidesOffPeople",
		+[](const Config& c) { return c.hands.bladeSlidesOffPeople ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.bladeSlidesOffPeople = v != 0.0f; },
	},
	{
		"Hands: physics", "Hands stop at walls",
		"Your free hand and your shield stop at walls, floors and furniture as the weapon does; pushed too far they let go",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "HandsStopAtWalls",
		+[](const Config& c) { return c.hands.handsStopAtWalls ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.handsStopAtWalls = v != 0.0f; },
	},
	{
		"Hands: physics", "Hands rest on people",
		"Held there slowly the free hand or the shield rests on a person; a fast hand is held or goes through, by the next setting",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "HandsStopAtBodies",
		+[](const Config& c) { return c.hands.handsStopAtBodies ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.handsStopAtBodies = v != 0.0f; },
	},
	{
		"Hands: physics", "Punches stop at people",
		"A punch, a slap or any fast hand is held at the person it meets and its blow still lands; pressed on past the let-go cap it goes in",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "PunchesStopAtBodies",
		+[](const Config& c) { return c.hands.punchesStopAtBodies ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.punchesStopAtBodies = v != 0.0f; },
	},
	{
		"Hands: physics", "Weapon weight (%)",
		"How far a heavy weapon trails the hand, by the item's own weight: 1 on the hand, 100 the heaviest",
		ItemKind::Number, 1.0f, 100.0f, 5.0f, 0, false,
		"Hands", "WeaponWeight",
		+[](const Config& c) { return c.hands.weaponWeightPercent; },
		+[](Config& c, float v) { c.hands.weaponWeightPercent = v; },
	},
	{
		"Hands: physics", "Weapon swing-through",
		"The weapon trails as with this off, and its momentum carries it past a hand that stops; off, it only trails",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "WeaponSwingThrough",
		+[](const Config& c) { return c.hands.weaponSwingThrough ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.weaponSwingThrough = v != 0.0f; },
	},
	{
		"Hands: physics", "Two-hander in one hand: weight (%)",
		"A two-handed sword, axe or hammer held in one hand trails this much more than in both; 0 as in both. Not a staff",
		ItemKind::Number, 0.0f, 100.0f, 10.0f, 0, false,
		"Hands", "OneHandTwoHanderWeight",
		+[](const Config& c) { return c.hands.oneHandTwoHander.weightPercent; },
		+[](Config& c, float v) { c.hands.oneHandTwoHander.weightPercent = v; },
	},
	{
		"Hands: physics", "Two-hander in one hand: fatigue (%)",
		"Attacks, blocks, swings, parries, shoves and grabs cost this much more fatigue while a two-hander is in one hand",
		ItemKind::Number, 0.0f, 100.0f, 10.0f, 0, false,
		"Hands", "OneHandTwoHanderFatigue",
		+[](const Config& c) { return c.hands.oneHandTwoHander.fatiguePercent; },
		+[](Config& c, float v) { c.hands.oneHandTwoHander.fatiguePercent = v; },
	},
	{
		"Hands: physics", "Holding a two-hander up costs",
		"A two-hander held over the hip in one hand burns fatigue; held low, resting on a shoulder or in both hands it does not",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "OneHandTwoHanderHoldCosts",
		+[](const Config& c) { return c.hands.oneHandTwoHander.holdCosts ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.oneHandTwoHander.holdCosts = v != 0.0f; },
	},
	{
		"Hands: physics", "Holding it up drains (a second)",
		"How fast the fatigue bar falls while a two-hander is held up in one hand; what the game gives back is burnt on top",
		ItemKind::Number, 0.0f, 40.0f, 1.0f, 0, false,
		"Hands", "OneHandTwoHanderHoldFatigue",
		+[](const Config& c) { return c.hands.oneHandTwoHander.holdPerSecond; },
		+[](Config& c, float v) { c.hands.oneHandTwoHander.holdPerSecond = v; },
	},
	// ---- Hands: melee
	{
		"Hands: melee", "Strikes by motion", "The swung blade hits what it passes through, no animation",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "MotionHits",
		+[](const Config& c) { return c.hands.motionHits ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.motionHits = v != 0.0f; },
	},
	{
		"Hands: melee", "Strikes train skills", "Your strikes train Blade, Blunt or Hand to Hand as the game's attacks do",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "StrikesTrainSkills",
		+[](const Config& c) { return c.hands.strikesTrainSkills ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.strikesTrainSkills = v != 0.0f; },
	},
	{
		"Hands: melee", "Swing speed", "How fast the hand must move (m/s) for a swing: only then it strikes and swishes",
		ItemKind::Number, 0.5f, 5.0f, 0.1f, 1, false,
		"Hands", "SwingLight",
		+[](const Config& c) { return c.hands.gestures.swingLight; },
		+[](Config& c, float v) { c.hands.gestures.swingLight = v; },
	},
	{
		"Hands: melee", "Strike size", "How much of a body's bound the blade has to reach, 0 to 1",
		ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false,
		"Hands", "HitBoundFactor",
		+[](const Config& c) { return c.hands.hitBoundFactor; },
		+[](Config& c, float v) { c.hands.hitBoundFactor = v; },
	},
	{
		"Hands: melee", "Hits need the touch",
		"A swing or a punch hits only where the blade or the fist touches the body, as in Blade & Sorcery; off, the game's generous reach",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "MotionHitsByTouch",
		+[](const Config& c) { return c.hands.hitsByTouch ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.hitsByTouch = v != 0.0f; },
	},
	{
		"Hands: melee", "Power strike speed",
		"A hit lands the moment the blade touches; this fast (m/s) at the touch it is a power attack. 0: only a long swing is",
		ItemKind::Number, 0.0f, 12.0f, 0.1f, 1, false,
		"Hands", "PowerStrikeSpeed",
		+[](const Config& c) { return c.hands.powerStrikeSpeed; },
		+[](Config& c, float v) { c.hands.powerStrikeSpeed = v; },
	},
	{
		"Hands: melee", "Swing length", "How far (m) a swing must travel before it strikes at all - shorter is a twitch",
		ItemKind::Number, 0.02f, 1.0f, 0.02f, 2, false,
		"Hands", "SwingMinMetres",
		+[](const Config& c) { return c.hands.gestures.swingMinMetres; },
		+[](Config& c, float v) { c.hands.gestures.swingMinMetres = v; },
	},
	{
		"Hands: melee", "Power swing length", "How far (m) a swing must travel to be a power attack - the same speed, a longer swing",
		ItemKind::Number, 0.2f, 2.0f, 0.05f, 2, false,
		"Hands", "PowerSwingMetres",
		+[](const Config& c) { return c.hands.gestures.powerSwingMetres; },
		+[](Config& c, float v) { c.hands.gestures.powerSwingMetres = v; },
	},
	{
		"Hands: melee", "Power thrust length", "How far (m) a thrust forward must go to be a power attack",
		ItemKind::Number, 0.2f, 1.0f, 0.05f, 2, false,
		"Hands", "PowerThrustMetres",
		+[](const Config& c) { return c.hands.gestures.powerThrustMetres; },
		+[](Config& c, float v) { c.hands.gestures.powerThrustMetres = v; },
	},
	{
		"Hands: melee", "Weapon parries",
		"Your blade meeting an attacker's during their swing blocks their blow - all of it - for a little fatigue",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "WeaponParries",
		+[](const Config& c) { return c.hands.weaponParries ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.weaponParries = v != 0.0f; },
	},
	{
		"Hands: melee", "Block by angle",
		"A raised weapon blocks only a blade crossing yours, a shield only a blow its face is turned to",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "BlockByAngle",
		+[](const Config& c) { return c.hands.blockByAngle ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.blockByAngle = v != 0.0f; },
	},
	{
		"Hands: melee", "Block: blade slack", "How far off a right angle the blades may cross and still block, degrees",
		ItemKind::Number, 5.0f, 90.0f, 5.0f, 0, false,
		"Hands", "BlockBladeDegrees",
		+[](const Config& c) { return c.hands.blockBladeDegrees; },
		+[](Config& c, float v) { c.hands.blockBladeDegrees = v; },
	},
	{
		"Hands: melee", "Block: shield slack", "How far the shield's face may turn away from the blow and still block, degrees",
		ItemKind::Number, 10.0f, 90.0f, 5.0f, 0, false,
		"Hands", "BlockShieldDegrees",
		+[](const Config& c) { return c.hands.blockShieldDegrees; },
		+[](Config& c, float v) { c.hands.blockShieldDegrees = v; },
	},
	{
		"Hands: melee", "Block: reach (units)",
		"How near their blade has to pass your blade or shield at the blow to be blocked (70 a metre); 0 asks no distance",
		ItemKind::Number, 0.0f, 150.0f, 5.0f, 0, false,
		"Hands", "BlockReachUnits",
		+[](const Config& c) { return c.hands.blockReachUnits; },
		+[](Config& c, float v) { c.hands.blockReachUnits = v; },
	},
	{
		"Hands: melee", "Two hands on two-handers", "Close the left grip on the handle to hold it with both hands",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "TwoHandGrip",
		+[](const Config& c) { return c.hands.twoHand.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.twoHand.enabled = v != 0.0f; },
	},
	{
		"Hands: melee", "Two hands on one-handers", "One-handed weapons can be held with both hands too",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "TwoHandOneHanders",
		+[](const Config& c) { return c.hands.twoHand.oneHanders ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.twoHand.oneHanders = v != 0.0f; },
	},
	{
		"Hands: melee", "Left hand slides on the handle", "Held with both hands, the left trigger held slides the hand along the handle",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "TwoHandSlide",
		+[](const Config& c) { return c.hands.twoHand.slide ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.twoHand.slide = v != 0.0f; },
	},
	{
		"Hands: melee", "Fists by a fist", "Full VR, no weapon: a fist raises the fists, an open hand lowers them",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "Fists",
		+[](const Config& c) { return c.hands.fist.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.fist.enabled = v != 0.0f; },
	},
	{
		"Hands: melee", "Fist at", "How far index to little finger must all curl to count as a fist (1 = fully)",
		ItemKind::Number, 0.4f, 0.95f, 0.05f, 2, false,
		"Hands", "FistCloseCurl",
		+[](const Config& c) { return c.hands.fist.closeCurl; },
		+[](Config& c, float v) { c.hands.fist.closeCurl = v; },
	},
	{
		"Hands: melee", "Fist by grip (force)",
		"A grip squeezed this hard is a fist as well, for a controller whose fingers do not read; 0 off",
		ItemKind::Number, 0.0f, 1.0f, 0.1f, 1, false,
		"Hands", "FistGripForce",
		+[](const Config& c) { return c.hands.fist.gripForce; },
		+[](Config& c, float v) { c.hands.fist.gripForce = v; },
	},
	// ---- Hands: bow
	{
		"Hands: bow", "Bow by hand", "An arrow from over the shoulder, nocked on the bow, pulled back and let go",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "BowByHand",
		+[](const Config& c) { return c.hands.archery.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.archery.enabled = v != 0.0f; },
	},
	{
		"Hands: bow", "Arrow with grip", "The bow by hand: take and hold the arrow with the grip instead of the trigger",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ArrowWithGrip",
		+[](const Config& c) { return c.hands.archery.takeWithTrigger ? 0.0f : 1.0f; },
		+[](Config& c, float v) { c.hands.archery.takeWithTrigger = v == 0.0f; },
	},
	{
		"Hands: bow", "Arrow stabs", "Thrust the arrow in your hand into someone, head first, to stab them",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ArrowStabs",
		+[](const Config& c) { return c.hands.archery.stabs ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.archery.stabs = v != 0.0f; },
	},
	{
		"Hands: bow", "Bow: nock reach", "How near, metres, the arrow must come to the string to go onto it",
		ItemKind::Number, 0.04f, 0.30f, 0.01f, 2, false,
		"Hands", "NockMetres",
		+[](const Config& c) { return c.hands.archery.nockMetres; },
		+[](Config& c, float v) { c.hands.archery.nockMetres = v; },
	},
	{
		"Hands: bow", "Bow: lead from", "From how far, metres, the hand is drawn towards the string; the nearer, the harder",
		ItemKind::Number, 0.0f, 0.80f, 0.05f, 2, false,
		"Hands", "BowGuideMetres",
		+[](const Config& c) { return c.hands.archery.guideMetres; },
		+[](Config& c, float v) { c.hands.archery.guideMetres = v; },
	},
	{
		"Hands: bow", "Bow: string's place", "How far, metres, behind the bow hand the string rests",
		ItemKind::Number, 0.0f, 0.40f, 0.01f, 2, false,
		"Hands", "BowStringMetres",
		+[](const Config& c) { return c.hands.archery.stringMetres; },
		+[](Config& c, float v) { c.hands.archery.stringMetres = v; },
	},
	{
		"Hands: bow", "Bow: draw from", "How far, metres, past the string's place the arrow is pulled before the draw starts",
		ItemKind::Number, 0.0f, 0.30f, 0.01f, 2, false,
		"Hands", "BowDrawStartMetres",
		+[](const Config& c) { return c.hands.archery.drawStartMetres; },
		+[](Config& c, float v) { c.hands.archery.drawStartMetres = v; },
	},
	{
		"Hands: bow", "Bow: arrow off from", "How far, metres, off the bow's line the arrow is taken off the string",
		ItemKind::Number, 0.04f, 0.40f, 0.01f, 2, false,
		"Hands", "BowUnnockMetres",
		+[](const Config& c) { return c.hands.archery.unnockMetres; },
		+[](Config& c, float v) { c.hands.archery.unnockMetres = v; },
	},
	{
		"Hands: bow", "Quiver size", "How big, metres, the place over the shoulder is where an arrow is taken",
		ItemKind::Number, 0.05f, 0.50f, 0.01f, 2, false,
		"Hands", "QuiverRadius",
		+[](const Config& c) { return c.hands.archery.quiverRadius; },
		+[](Config& c, float v) { c.hands.archery.quiverRadius = v; },
	},
	// ---- Hands: people
	{
		"Hands: people", "Shove", "An open hand driven fast into someone, weapons away, shoves them: they stagger, or fall",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "Shove",
		+[](const Config& c) { return c.hands.shove.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.shove.enabled = v != 0.0f; },
	},
	{
		"Hands: people", "Shove speed", "How fast (m/s) the hand must move into someone for a shove",
		ItemKind::Number, 0.5f, 6.0f, 0.1f, 1, false,
		"Hands", "ShoveSpeed",
		+[](const Config& c) { return c.hands.shove.speed; },
		+[](Config& c, float v) { c.hands.shove.speed = v; },
	},
	{
		"Hands: people", "Hard shove speed", "From this speed (m/s) a shove knocks them down",
		ItemKind::Number, 1.0f, 10.0f, 0.1f, 1, false,
		"Hands", "ShoveHardSpeed",
		+[](const Config& c) { return c.hands.shove.hardSpeed; },
		+[](Config& c, float v) { c.hands.shove.hardSpeed = v; },
	},
	{
		"Hands: people", "Shove down counts as a hit", "Shoving someone to the ground is taken like a blow: friends forgive a few, others call the guards",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ShoveCountsAsHit",
		+[](const Config& c) { return c.hands.shove.countsAsHit ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.shove.countsAsHit = v != 0.0f; },
	},
	{
		"Hands: people", "Slap disposition", "How much a slap in the face (a light shove landed high) costs their liking",
		ItemKind::Number, 0.0f, 50.0f, 1.0f, 0, false,
		"Hands", "SlapDisposition",
		+[](const Config& c) { return c.hands.shove.dispositionFace; },
		+[](Config& c, float v) { c.hands.shove.dispositionFace = v; },
	},
	{
		"Hands: people", "Middle finger", "The middle finger held at someone costs their liking",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "MiddleFinger",
		+[](const Config& c) { return c.hands.middleFinger ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.middleFinger = v != 0.0f; },
	},
	{
		"Hands: people", "Middle finger disposition", "How much the middle finger costs their liking",
		ItemKind::Number, 0.0f, 50.0f, 1.0f, 0, false,
		"Hands", "MiddleFingerDisposition",
		+[](const Config& c) { return c.hands.middleFingerDisposition; },
		+[](Config& c, float v) { c.hands.middleFingerDisposition = v; },
	},
	{
		"Hands: people", "Take by the hand", "Close a grip next to someone's hand and they walk after you",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "LeadByHand",
		+[](const Config& c) { return c.hands.lead.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.lead.enabled = v != 0.0f; },
	},
	{
		"Hands: people", "Strangers follow for", "How far someone who is not your follower lets you lead them, metres",
		ItemKind::Number, 1.0f, 20.0f, 0.5f, 1, false,
		"Hands", "LeadStrangerMetres",
		+[](const Config& c) { return c.hands.lead.strangerMetres; },
		+[](Config& c, float v) { c.hands.lead.strangerMetres = v; },
	},
	{
		"Hands: people", "Off hand strikes",
		"With a weapon drawn, the free hand punched or driven fast into someone staggers them, or knocks them down",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "OffHandStrikes",
		+[](const Config& c) { return c.hands.grapple.strikes ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.grapple.strikes = v != 0.0f; },
	},
	{
		"Hands: people", "Off hand grabs fighters",
		"The free hand's grip on a fighter holds them: they stop, follow the hand, and held by the head or weapon "
		"arm cannot strike; let go fast to throw them",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "OffHandGrabs",
		+[](const Config& c) { return c.hands.grapple.grabs ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.grapple.grabs = v != 0.0f; },
	},
	{
		"Hands: people", "Held for (s)",
		"How long a grabbed fighter stays held before they break free",
		ItemKind::Number, 0.5f, 30.0f, 0.5f, 1, false,
		"Hands", "GrabHoldSeconds",
		+[](const Config& c) { return c.hands.grapple.holdSeconds; },
		+[](Config& c, float v) { c.hands.grapple.holdSeconds = v; },
	},
	{
		"Hands: people", "Grab takes them down",
		"Grabbed by the torso, an arm or a leg a fighter falls into their ragdoll and hangs from the hand; let go "
		"fast they fly. Off, they stay standing. The head and the neck always hold them standing",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "GrabRagdoll",
		+[](const Config& c) { return c.hands.grapple.ragdoll ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.grapple.ragdoll = v != 0.0f; },
	},
	{
		"Hands: people", "Grab throw strength",
		"A fighter held in their ragdoll and let go fast flies with the hand's speed times this; 0 drops them",
		ItemKind::Number, 0.0f, 5.0f, 0.25f, 2, false,
		"Hands", "GrabThrowStrength",
		+[](const Config& c) { return c.hands.grapple.throwStrength; },
		+[](Config& c, float v) { c.hands.grapple.throwStrength = v; },
	},
	{
		"Hands: people", "Grab weight (x)",
		"How heavy a fighter held in their ragdoll is: your Strength pulls against their Havok masses, so they lag "
		"behind the hand and sag under it, and one too heavy to lift is dragged. 0 is weightless, 2 twice as heavy",
		ItemKind::Number, 0.0f, 5.0f, 0.25f, 2, false,
		"Hands", "GrabWeight",
		+[](const Config& c) { return c.hands.grapple.weight; },
		+[](Config& c, float v) { c.hands.grapple.weight = v; },
	},
	// ---- Hands: enemies
	{
		"Hands: enemies", "Combat reach (units)",
		"How close fighters come to strike: 85 brings their blades within your reach; 0 is the game's own 128",
		ItemKind::Number, 0.0f, 200.0f, 5.0f, 0, false,
		"Hands", "CombatReach",
		+[](const Config& c) { return c.hands.combatReach; },
		+[](Config& c, float v) { c.hands.combatReach = v; },
	},
	{
		"Hands: enemies", "Hits push enemies (%)",
		"How far your hits shove the one you hit, as a share of the game's own knockback: 100 the game's, 0 they stay where they stand, as in Blade & Sorcery",
		ItemKind::Number, 0.0f, 100.0f, 10.0f, 0, false,
		"Hands", "EnemyKnockback",
		+[](const Config& c) { return c.hands.enemyKnockbackPercent; },
		+[](Config& c, float v) { c.hands.enemyKnockbackPercent = v; },
	},
	{
		"Hands: enemies", "Only power attacks push",
		"A light hit shoves nobody; a power attack pushes by the share above",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "EnemyKnockbackPowerOnly",
		+[](const Config& c) { return c.hands.enemyKnockbackPowerOnly ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.enemyKnockbackPowerOnly = v != 0.0f; },
	},
	{
		"Hands: enemies", "Enemies walk near you",
		"Fighters run up as in the game, and once near you they only walk - a slower fight, as in Blade & Sorcery",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "SlowApproach",
		+[](const Config& c) { return c.hands.slowApproach ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.slowApproach = v != 0.0f; },
	},
	{
		"Hands: enemies", "Walk within (m)",
		"How near (m) fighters have to be before they only walk",
		ItemKind::Number, 2.0f, 50.0f, 1.0f, 0, false,
		"Hands", "SlowApproachMetres",
		+[](const Config& c) { return c.hands.slowApproachMetres; },
		+[](Config& c, float v) { c.hands.slowApproachMetres = v; },
	},
	{
		"Hands: enemies", "Fighters' ring",
		"Those fighting you wait at a ring and walk round you; only so many step in to fight at once - as in Blade & Sorcery",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "CombatRing",
		+[](const Config& c) { return c.hands.combatRing ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.combatRing = v != 0.0f; },
	},
	{
		"Hands: enemies", "Ring distance (m)",
		"How far off (m) the waiting fighters keep; 0 turns the ring off",
		ItemKind::Number, 0.0f, 10.0f, 0.5f, 1, false,
		"Hands", "CombatRingMetres",
		+[](const Config& c) { return c.hands.combatRingMetres; },
		+[](Config& c, float v) { c.hands.combatRingMetres = v; },
	},
	{
		"Hands: enemies", "Attackers at once",
		"How many fighters may fight you at once (1-5); the rest wait at the ring",
		ItemKind::Number, 1.0f, 5.0f, 1.0f, 0, false,
		"Hands", "MeleeAttackersAtOnce",
		+[](const Config& c) { return static_cast<float>(c.hands.meleeAttackersAtOnce); },
		+[](Config& c, float v) { c.hands.meleeAttackersAtOnce = static_cast<int>(v + 0.5f); },
	},
	{
		"Hands: enemies", "Turn length (s)",
		"How long a fighter who steps in has at least, before the next one's turn",
		ItemKind::Number, 2.0f, 30.0f, 1.0f, 0, false,
		"Hands", "CombatRingTurnSeconds",
		+[](const Config& c) { return c.hands.combatRingTurnSeconds; },
		+[](Config& c, float v) { c.hands.combatRingTurnSeconds = v; },
	},
	{
		"Hands: enemies", "Turn length at most (s)",
		"Each turn lasts a random time between the turn length and this",
		ItemKind::Number, 2.0f, 60.0f, 1.0f, 0, false,
		"Hands", "CombatRingTurnMaxSeconds",
		+[](const Config& c) { return c.hands.combatRingTurnMaxSeconds; },
		+[](Config& c, float v) { c.hands.combatRingTurnMaxSeconds = v; },
	},
	{
		"Hands: enemies", "Weakest fight first",
		"The next turn goes to the lowest-level fighter waiting; the strongest come last (off: who waited longest)",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "CombatRingByLevel",
		+[](const Config& c) { return c.hands.combatRingByLevel ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.combatRingByLevel = v != 0.0f; },
	},
	{
		"Hands: enemies", "Waiting don't swing",
		"Fighters waiting at the ring for their turn do not swing at the air from afar",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "CombatRingHoldsBlows",
		+[](const Config& c) { return c.hands.combatRingHoldsBlows ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.combatRingHoldsBlows = v != 0.0f; },
	},
	{
		"Hands: enemies", "Attack tempo (%)",
		"How fast the attacks of those fighting you play, per cent of the game's own: slower, their wind-up "
		"can be seen and met",
		ItemKind::Number, 20.0f, 200.0f, 5.0f, 0, false,
		"Hands", "EnemyAttackSpeed",
		+[](const Config& c) { return c.hands.enemyAttackSpeed; },
		+[](Config& c, float v) { c.hands.enemyAttackSpeed = v; },
	},
	{
		"Hands: enemies", "Attack frequency (%)",
		"How often those fighting you choose to attack, per cent of the game's own: the chance of an attack, "
		"and the pause between their decisions the other way",
		ItemKind::Number, 10.0f, 200.0f, 5.0f, 0, false,
		"Hands", "EnemyAttackFrequency",
		+[](const Config& c) { return c.hands.enemyAttackFrequency; },
		+[](Config& c, float v) { c.hands.enemyAttackFrequency = v; },
	},
	// ---- Hands: reaching
	{
		"Hands: reaching", "Reach tooltip", "The tooltip icon moves onto what a closed grip would pick up",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ReachTooltip",
		+[](const Config& c) { return c.hands.reachTooltip ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.reachTooltip = v != 0.0f; },
	},
	{
		"Hands: reaching", "Reach ring", "A light-brown ring around what a closed grip would pick up",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ReachRing",
		+[](const Config& c) { return c.hands.reachRing ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.reachRing = v != 0.0f; },
	},
	{
		"Hands: reaching", "Reach marker distance", "How close a hand has to be for the ring to show, metres",
		ItemKind::Number, 0.05f, 3.0f, 0.05f, 2, false,
		"Hands", "ReachMarkerMetres",
		+[](const Config& c) { return c.hands.reachMarkerMetres; },
		+[](Config& c, float v) { c.hands.reachMarkerMetres = v; },
	},
	{
		"Hands: reaching", "Ring to the near side", "How near the hand the ring sits on the item's nearest side, metres",
		ItemKind::Number, 0.0f, 2.0f, 0.05f, 2, false,
		"Hands", "ReachNearSideMetres",
		+[](const Config& c) { return c.hands.reachNearSideMetres; },
		+[](Config& c, float v) { c.hands.reachNearSideMetres = v; },
	},
	{
		"Hands: reaching", "Reach marker opacity", "How solid the ring is, 0 to 1",
		ItemKind::Number, 0.1f, 1.0f, 0.05f, 2, false,
		"Hands", "ReachMarkerOpacity",
		+[](const Config& c) { return c.hands.reachMarkerOpacity; },
		+[](Config& c, float v) { c.hands.reachMarkerOpacity = v; },
	},
	{
		"Hands: reaching", "Target outline", "What a grip would take gets a light outline",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "TargetOutline",
		+[](const Config& c) { return c.hands.targetOutline ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.targetOutline = v != 0.0f; },
	},
	{
		"Hands: reaching", "Target glow", "What a grip would take glows softly",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "TargetGlow",
		+[](const Config& c) { return c.hands.targetGlow ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.targetGlow = v != 0.0f; },
	},
	{
		"Hands: reaching", "Open by reaching",
		"An open, empty hand at a chest, a body or - sneaking - a pocket opens it; the hand away closes it",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ReachOpens",
		+[](const Config& c) { return c.hands.reachOpen.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.reachOpen.enabled = v != 0.0f; },
	},
	{
		"Hands: reaching", "Reach to open (m)",
		"How near the hand has to come to a chest, a body or a pocket to open it",
		ItemKind::Number, 0.02f, 0.40f, 0.02f, 2, false,
		"Hands", "ReachOpenMetres",
		+[](const Config& c) { return c.hands.reachOpen.openMetres; },
		+[](Config& c, float v) { c.hands.reachOpen.openMetres = v; },
	},
	{
		"Hands: reaching", "Away to close (m)",
		"How far the hand has to go from what it opened to close it again",
		ItemKind::Number, 0.20f, 1.50f, 0.05f, 2, false,
		"Hands", "ReachCloseMetres",
		+[](const Config& c) { return c.hands.reachOpen.closeMetres; },
		+[](Config& c, float v) { c.hands.reachOpen.closeMetres = v; },
	},
	{
		"Hands: reaching", "Walk away to close (m)",
		"Once you walk with a reached menu open, only going this far from it closes it; the sticks still move you",
		ItemKind::Number, 0.5f, 5.0f, 0.1f, 1, false,
		"Hands", "ReachWalkAwayMetres",
		+[](const Config& c) { return c.hands.reachOpen.walkAwayMetres; },
		+[](Config& c, float v) { c.hands.reachOpen.walkAwayMetres = v; },
	},
	{
		"Hands: reaching", "Tooltip size", "Full VR: the crosshair and its icon on the controller, times the seated size",
		ItemKind::Number, 0.5f, 5.0f, 0.25f, 2, false,
		"Hands", "TooltipScale",
		+[](const Config& c) { return c.hands.tooltipScale; },
		+[](Config& c, float v) { c.hands.tooltipScale = v; },
	},
	// ---- Hands: laser
	{
		"Hands: laser", "Laser in the world", "The weapon hand's laser drawn in the world too, up to what it points at",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "LaserInWorld",
		+[](const Config& c) { return c.hands.laserInWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.laserInWorld = v != 0.0f; },
	},
	{
		"Hands: laser", "World laser length", "How long the laser in the world is when it points at nothing, metres",
		ItemKind::Number, 0.5f, 10.0f, 0.5f, 1, false,
		"Hands", "WorldLaserMetres",
		+[](const Config& c) { return c.hands.worldLaserMetres; },
		+[](Config& c, float v) { c.hands.worldLaserMetres = v; },
	},
	{
		"Hands: laser", "Laser beam", "Draw the beam from the hand that points at a menu",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "LaserBeam",
		+[](const Config& c) { return c.hands.laserBeam ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.laserBeam = v != 0.0f; },
	},
	{
		"Hands: laser", "Laser dot", "A dot at the end of the laser; the beam or the dot has to stay on",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "LaserDot",
		+[](const Config& c) { return c.hands.laserDot ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.laserDot = v != 0.0f; },
	},
	{
		"Hands: laser", "Laser places the cursor",
		"The game's menu cursor is put exactly where the laser points, before the menu looks; off: walked there by mouse steps",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "LaserCursorDirect",
		+[](const Config& c) { return c.hands.laserCursorDirect ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.laserCursorDirect = v != 0.0f; },
	},
	{
		"Hands: laser", "Button sticks (pixels)",
		"On menus of buttons alone the button under the beam stays lit while the beam wobbles less than this; 0 off",
		ItemKind::Number, 0.0f, 200.0f, 10.0f, 0, false,
		"Hands", "LaserStickyPixels",
		+[](const Config& c) { return c.hands.laserStickyPixels; },
		+[](Config& c, float v) { c.hands.laserStickyPixels = v; },
	},
	{
		"Hands: laser", "Laser picks items", "Off: the laser leaves takeable things alone - items are the hands' (reach, pull, stow); doors, people and the rest still take the laser",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "LaserPicksItems",
		+[](const Config& c) { return c.hands.laserPicksItems ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.laserPicksItems = v != 0.0f; },
	},
	{
		"Hands: laser", "Laser tilt", "Degrees the laser leaves the controller turned down",
		ItemKind::Number, -90.0f, 90.0f, 5.0f, 0, false,
		"Hands", "LaserPitchDegrees",
		+[](const Config& c) { return c.hands.laserPitchDegrees; },
		+[](Config& c, float v) { c.hands.laserPitchDegrees = v; },
	},
	{
		"Hands: laser", "Laser sideways", "Where the laser starts: metres right of the controller (left hand mirrored)",
		ItemKind::Number, -0.1f, 0.1f, 0.005f, 3, false,
		"Hands", "LaserRightMetres",
		+[](const Config& c) { return c.hands.laserOffsetRightMetres; },
		+[](Config& c, float v) { c.hands.laserOffsetRightMetres = v; },
	},
	{
		"Hands: laser", "Laser height", "Where the laser starts: metres above the controller",
		ItemKind::Number, -0.1f, 0.1f, 0.005f, 3, false,
		"Hands", "LaserUpMetres",
		+[](const Config& c) { return c.hands.laserOffsetUpMetres; },
		+[](Config& c, float v) { c.hands.laserOffsetUpMetres = v; },
	},
	{
		"Hands: laser", "Laser start", "Where the laser starts: metres along it from the controller (negative: back)",
		ItemKind::Number, -0.2f, 0.2f, 0.01f, 2, false,
		"Hands", "LaserOriginMetres",
		+[](const Config& c) { return c.hands.laserOriginMetres; },
		+[](Config& c, float v) { c.hands.laserOriginMetres = v; },
	},
	{
		"Hands: laser", "Crosshair place", "laser: ahead of the pointing hand. target: over the thing pointed at, facing you. view: straight ahead",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kCrosshairPlaceCount - 1), 1.0f, 0, false,
		"Hands", "CrosshairPlace",
		+[](const Config& c) { return static_cast<float>(c.hands.crosshairPlace); },
		+[](Config& c, float v) { c.hands.crosshairPlace = vr::CrosshairPlaceFromIndex(v); },
		"", "", SettingAction::None, vr::kCrosshairPlaceNames, vr::kCrosshairPlaceCount,
	},
	{
		"Hands: laser", "Sneak indicator",
		"Full VR while sneaking: vignette - a ring at the edge, black unseen, turning white when seen. eye: the game's eye above the view",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kSneakIndicatorCount - 1), 1.0f, 0, false,
		"Hands", "SneakIndicator",
		+[](const Config& c) { return static_cast<float>(c.hands.sneakIndicator); },
		+[](Config& c, float v) { c.hands.sneakIndicator = vr::SneakIndicatorFromIndex(v); },
		"", "", SettingAction::None, vr::kSneakIndicatorNames, vr::kSneakIndicatorCount,
	},
	{
		"Hands: laser", "Sneak vignette strength", "How strong the sneak vignette is (black unseen, white seen)",
		ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false,
		"Hands", "SneakVignetteStrength",
		+[](const Config& c) { return c.hands.sneakVignetteStrength; },
		+[](Config& c, float v) { c.hands.sneakVignetteStrength = v; },
	},
	// ---- Hands: menus
	{
		"Hands: menus", "HUD on the wrist", "The HUD hangs on the right wrist",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "WristHud",
		+[](const Config& c) { return c.hands.wristHud ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.wristHud = v != 0.0f; },
	},
	{
		"Hands: menus", "HUD size on the wrist", "Width of the wrist HUD in metres",
		ItemKind::Number, 0.10f, 1.00f, 0.05f, 2, false,
		"Hands", "WristHudWidth",
		+[](const Config& c) { return c.hands.wristHudWidth; },
		+[](Config& c, float v) { c.hands.wristHudWidth = v; },
	},
	{
		"Hands: menus", "Menus on the wrist", "The Tab menus hang on a wrist",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "WristMenu",
		+[](const Config& c) { return c.hands.wristMenu ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.wristMenu = v != 0.0f; },
	},
	{
		"Hands: menus", "Menu size on the wrist", "Width of the wrist menu in metres",
		ItemKind::Number, 0.20f, 1.50f, 0.05f, 2, false,
		"Hands", "WristMenuWidth",
		+[](const Config& c) { return c.hands.wristMenuWidth; },
		+[](Config& c, float v) { c.hands.wristMenuWidth = v; },
	},
	{
		"Hands: menus", "Menus on the right hand", "On: right wrist, the left hand presses. Off: the other way",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "MenuOnRight",
		+[](const Config& c) { return c.hands.menuOnRight ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.menuOnRight = v != 0.0f; },
	},
	{
		"Hands: menus", "Hands stay in menus",
		"Your hands stay in the inventory, containers, the map, books and dialogue; only the Esc menu takes them away",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "HandsInMenus",
		+[](const Config& c) { return c.hands.handsInMenus ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.handsInMenus = v != 0.0f; },
	},
	{
		"Hands: menus", "Menu haptics", "A tick in the controller at every button the laser comes onto, a firmer one as it clicks",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "MenuHaptics",
		+[](const Config& c) { return c.hands.menuHaptics ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.menuHaptics = v != 0.0f; },
	},
	{
		"Hands: menus", "First person after a load", "A save made in third person is put into first person once after it loads; a view chosen later stays",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "FirstPersonAfterLoad",
		+[](const Config& c) { return c.hands.firstPersonAfterLoad ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.firstPersonAfterLoad = v != 0.0f; },
	},
	{
		"Hands: menus", "VR keyboard", "SteamVR's keyboard opens for the game's text fields: a name, a spell, an enchantment, a potion",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "VrKeyboard",
		+[](const Config& c) { return c.hands.vrKeyboard ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.vrKeyboard = v != 0.0f; },
	},
	{
		"Hands: menus", "Quick menu", "Full VR: hold the right trackpad, move to a hotkey, let go; or click the right stick, move, click again",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "QuickMenu",
		+[](const Config& c) { return c.hands.quickMenu.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.quickMenu.enabled = v != 0.0f; },
	},
	{
		"Hands: menus", "Spell ring", "Full VR: click the other stick, move to a spell, click again - the spells you know, eight a page, the trigger turns them",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "SpellRing",
		+[](const Config& c) { return c.hands.spellRing ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.spellRing = v != 0.0f; },
	},
	{
		"Hands: menus", "Stick hold", "Seconds a stick's button is held: the right for the rest menu, the left for the run; a shorter click is a ring",
		ItemKind::Number, 0.3f, 2.0f, 0.1f, 1, false,
		"Hands", "StickHoldSeconds",
		+[](const Config& c) { return c.hands.stickHoldSeconds; },
		+[](Config& c, float v) { c.hands.stickHoldSeconds = v; },
	},
	// ---- Hands: holsters
	{
		"Hands: holsters", "Draw by reaching", "Full VR: four places - the hips and the shoulders - each keep one thing; the weapon hand for weapons, the other for the bow, shield and torch",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "Holsters",
		+[](const Config& c) { return c.hands.holster.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.holster.enabled = v != 0.0f; },
	},
	{
		"Hands: holsters", "One-hander side", "Which hip the one-handed weapon hangs on; auto: the left, the right when left-handed",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kHolsterSideCount - 1), 1.0f, 0, false,
		"Hands", "HolsterOneHandSide",
		+[](const Config& c) { return static_cast<float>(c.hands.holster.oneHandSide); },
		+[](Config& c, float v) { c.hands.holster.oneHandSide = vr::HolsterSideFromIndex(v); },
		"", "", SettingAction::None, vr::kHolsterSideNames, vr::kHolsterSideCount,
	},
	{
		"Hands: holsters", "Two-hander side", "Which shoulder the two-hander and staff hang over; auto: the right, the left when left-handed",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kHolsterSideCount - 1), 1.0f, 0, false,
		"Hands", "HolsterTwoHandSide",
		+[](const Config& c) { return static_cast<float>(c.hands.holster.twoHandSide); },
		+[](Config& c, float v) { c.hands.holster.twoHandSide = vr::HolsterSideFromIndex(v); },
		"", "", SettingAction::None, vr::kHolsterSideNames, vr::kHolsterSideCount,
	},
	{
		"Hands: holsters", "Bow side", "Which shoulder the bow sits behind; auto: the left, the right when left-handed",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kHolsterSideCount - 1), 1.0f, 0, false,
		"Hands", "HolsterBowSide",
		+[](const Config& c) { return static_cast<float>(c.hands.holster.bowSide); },
		+[](Config& c, float v) { c.hands.holster.bowSide = vr::HolsterSideFromIndex(v); },
		"", "", SettingAction::None, vr::kHolsterSideNames, vr::kHolsterSideCount,
	},
	{
		"Hands: holsters", "Fit weapon places", "Show where one- and two-handed weapons hang and the bow sits, with the triggers",
		ItemKind::Action, 0.0f, 0.0f, 0.0f, 0, false,
		"", "",
		+[](const Config&) { return 0.0f; },
		+[](Config&, float) {},
		"", "", SettingAction::FitHolsters,
	},
	// ---- Hands: moving
	{
		"Hands: moving", "Stick turn speed", "How fast the right stick turns you: mouse pixels a frame at full deflection",
		ItemKind::Number, 1.0f, 60.0f, 1.0f, 0, false,
		"Hands", "TurnSpeed",
		+[](const Config& c) { return c.hands.turnSpeed; },
		+[](Config& c, float v) { c.hands.turnSpeed = v; },
	},
	{
		"Hands: moving", "Toggle run", "A left stick click switches running on and off, instead of holding it in",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "RunToggle",
		+[](const Config& c) { return c.hands.runToggle ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.runToggle = v != 0.0f; },
	},
	{
		"Hands: moving", "Run by swinging arms", "Swing both arms as when running to run; the left stick held no longer runs",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ArmSwingRun",
		+[](const Config& c) { return c.hands.armSwing.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.armSwing.enabled = v != 0.0f; },
	},
	{
		"Hands: moving", "Walk direction", "The left stick walks where the head, a hand, or head and stick hand together point",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kWalkDirectionCount - 1), 1.0f, 0, false,
		"Hands", "WalkDirection",
		+[](const Config& c) { return static_cast<float>(c.hands.walkDirection); },
		+[](Config& c, float v) { c.hands.walkDirection = vr::WalkDirectionFromIndex(v); },
		"", "", SettingAction::None, vr::kWalkDirectionNames, vr::kWalkDirectionCount,
	},
	{
		"Hands: moving", "Hold to sneak", "Sneak while the right stick is held down; off, a flick down toggles it",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "SneakHold",
		+[](const Config& c) { return c.hands.sneakHold ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.sneakHold = v != 0.0f; },
	},
	{
		"Hands: moving", "Crouch to sneak", "Crouching in the room sneaks in the game, standing up ends it",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "CrouchSneak",
		+[](const Config& c) { return c.hands.crouchSneak ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.crouchSneak = v != 0.0f; },
	},
	{
		"Hands: moving", "Crouch depth", "How far (m) the head must drop under its standing height to count as a crouch",
		ItemKind::Number, 0.1f, 0.6f, 0.05f, 2, false,
		"Hands", "CrouchDropMetres",
		+[](const Config& c) { return c.hands.crouchDropMetres; },
		+[](Config& c, float v) { c.hands.crouchDropMetres = v; },
	},
	{
		"Hands: moving", "Sneak keeps eye height", "Crouching in the room, the game's sneak no longer lowers the view on top; a sneak by the stick still does",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "KeepEyeHeightSneaking",
		+[](const Config& c) { return c.hands.keepEyeHeightSneaking ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.keepEyeHeightSneaking = v != 0.0f; },
	},
	// ---- HUD on the hands (Full VR) -----------------------------------------
	{
		"HUD", "HUD on the hands", "Parts of the HUD on your hands, the compass in the sky",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"HandHud", "Enabled",
		+[](const Config& c) { return c.hands.handHud.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.handHud.enabled = v != 0.0f; },
	},
	{
		"HUD", "Show when looked at",
		"Off: always shown, on the back of your hand. On: only while you look at it, palm to your face",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"HandHud", "ShowOnLook",
		+[](const Config& c) { return c.hands.handHud.showOnLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.handHud.showOnLook = v != 0.0f; },
	},
	{
		"HUD", "Look: hand in view", "How close to the middle of your view the hand must be, degrees",
		ItemKind::Number, 10.0f, 80.0f, 5.0f, 0, false,
		"HandHud", "LookGazeDegrees",
		+[](const Config& c) { return c.hands.handHud.lookGazeDegrees; },
		+[](Config& c, float v) { c.hands.handHud.lookGazeDegrees = v; },
	},
	{
		"HUD", "Look: palm turned", "How squarely the palm must face you, degrees off",
		ItemKind::Number, 10.0f, 90.0f, 5.0f, 0, false,
		"HandHud", "LookPalmDegrees",
		+[](const Config& c) { return c.hands.handHud.lookPalmDegrees; },
		+[](Config& c, float v) { c.hands.handHud.lookPalmDegrees = v; },
	},
	{
		"HUD", "Look: fade", "How long the hand HUD takes to fade in and out, seconds",
		ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false,
		"HandHud", "LookFadeSeconds",
		+[](const Config& c) { return c.hands.handHud.lookFadeSeconds; },
		+[](Config& c, float v) { c.hands.handHud.lookFadeSeconds = v; },
	},
	{
		"HUD", "Look: off the palm", "How far off the palm the HUD hangs, metres",
		ItemKind::Number, 0.0f, 0.2f, 0.01f, 2, false,
		"HandHud", "PalmLiftMetres",
		+[](const Config& c) { return c.hands.handHud.palmLiftMetres; },
		+[](Config& c, float v) { c.hands.handHud.palmLiftMetres = v; },
	},
	{
		"HUD", "Look: down the palm", "How far below the controller's top the HUD lies in the palm, metres",
		ItemKind::Number, -0.1f, 0.2f, 0.01f, 2, false,
		"HandHud", "PalmDownMetres",
		+[](const Config& c) { return c.hands.handHud.palmDownMetres; },
		+[](Config& c, float v) { c.hands.handHud.palmDownMetres = v; },
	},
	{
		"HUD", "Hand HUD X", "The HUD on the hands moved to the right as you see it, metres (negative: left)",
		ItemKind::Number, -0.15f, 0.15f, 0.01f, 2, false,
		"HandHud", "OffsetRightMetres",
		+[](const Config& c) { return c.hands.handHud.offsetRightMetres; },
		+[](Config& c, float v) { c.hands.handHud.offsetRightMetres = v; },
	},
	{
		"HUD", "Hand HUD Y", "The HUD on the hands moved up as you see it, metres (negative: down)",
		ItemKind::Number, -0.15f, 0.15f, 0.01f, 2, false,
		"HandHud", "OffsetUpMetres",
		+[](const Config& c) { return c.hands.handHud.offsetUpMetres; },
		+[](Config& c, float v) { c.hands.handHud.offsetUpMetres = v; },
	},
	OBVR_HUD_ELEMENT_ROWS(0, "Health bars", "Bars")
	OBVR_HUD_ELEMENT_ROWS(1, "Spell", "Spell")
	OBVR_HUD_ELEMENT_ROWS(2, "Weapon", "Weapon")
	OBVR_HUD_ELEMENT_ROWS(3, "Active effects", "Effects")
	OBVR_HUD_ELEMENT_ROWS(4, "Level-up icon", "LevelUp")
	OBVR_HUD_ELEMENT_ROWS(5, "Compass", "Compass")
	OBVR_HUD_ELEMENT_ROWS(6, "Region name", "Region")
	OBVR_HUD_ELEMENT_ROWS(7, "Messages", "Messages")
	OBVR_HUD_ELEMENT_ROWS(8, "Subtitles", "Subtitles")
	OBVR_HUD_ELEMENT_ROWS(9, "Target info", "Info")
	{
		"HUD", "Text distance", "How far away messages and subtitles hang, metres",
		ItemKind::Number, 0.5f, 3.0f, 0.1f, 1, false,
		"HandHud", "ViewDistanceMetres",
		+[](const Config& c) { return c.hands.handHud.viewDistanceMetres; },
		+[](Config& c, float v) { c.hands.handHud.viewDistanceMetres = v; },
	},
	{
		"HUD", "Top of view", "How far above the middle of the view the top row hangs, degrees",
		ItemKind::Number, 0.0f, 45.0f, 1.0f, 0, false,
		"HandHud", "ViewTopDegrees",
		+[](const Config& c) { return c.hands.handHud.viewTopDegrees; },
		+[](Config& c, float v) { c.hands.handHud.viewTopDegrees = v; },
	},
	{
		"HUD", "Bottom of view", "How far below the middle of the view the bottom row hangs, degrees",
		ItemKind::Number, 0.0f, 45.0f, 1.0f, 0, false,
		"HandHud", "ViewBottomDegrees",
		+[](const Config& c) { return c.hands.handHud.viewBottomDegrees; },
		+[](Config& c, float v) { c.hands.handHud.viewBottomDegrees = v; },
	},
	{
		"HUD", "Text follows after", "Turn this far away and the text comes back to the middle, degrees; 0 never",
		ItemKind::Number, 0.0f, 90.0f, 5.0f, 0, false,
		"HandHud", "ViewFollowDegrees",
		+[](const Config& c) { return c.hands.handHud.viewFollowDegrees; },
		+[](Config& c, float v) { c.hands.handHud.viewFollowDegrees = v; },
	},
	{
		"HUD", "Text follow time", "How slowly the text glides after the head, seconds; 0 jumps at once",
		ItemKind::Number, 0.0f, 2.0f, 0.1f, 1, false,
		"HandHud", "ViewFollowSeconds",
		+[](const Config& c) { return c.hands.handHud.viewFollowSeconds; },
		+[](Config& c, float v) { c.hands.handHud.viewFollowSeconds = v; },
	},
	{
		"HUD", "Text locked to the head", "Messages and subtitles ride the head instead",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"HandHud", "ViewLockedToHead",
		+[](const Config& c) { return c.hands.handHud.viewLockedToHead ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.handHud.viewLockedToHead = v != 0.0f; },
	},
	{
		"HUD", "Compass height", "How far above the horizon the compass hangs, degrees",
		ItemKind::Number, 15.0f, 85.0f, 5.0f, 0, false,
		"HandHud", "CompassElevationDegrees",
		+[](const Config& c) { return c.hands.handHud.compassElevationDegrees; },
		+[](Config& c, float v) { c.hands.handHud.compassElevationDegrees = v; },
	},
	{
		"HUD", "Compass fade start", "Looking this far up, degrees, the compass starts to show",
		ItemKind::Number, 0.0f, 80.0f, 5.0f, 0, false,
		"HandHud", "CompassFadeStartDegrees",
		+[](const Config& c) { return c.hands.handHud.compassFadeStartDegrees; },
		+[](Config& c, float v) { c.hands.handHud.compassFadeStartDegrees = v; },
	},
	{
		"HUD", "Compass fade full", "From this far up, degrees, it shows fully",
		ItemKind::Number, 0.0f, 85.0f, 5.0f, 0, false,
		"HandHud", "CompassFadeFullDegrees",
		+[](const Config& c) { return c.hands.handHud.compassFadeFullDegrees; },
		+[](Config& c, float v) { c.hands.handHud.compassFadeFullDegrees = v; },
	},
	{
		"HUD", "Compass distance", "How far away the compass hangs, metres",
		ItemKind::Number, 0.5f, 5.0f, 0.1f, 1, false,
		"HandHud", "CompassDistanceMetres",
		+[](const Config& c) { return c.hands.handHud.compassDistanceMetres; },
		+[](Config& c, float v) { c.hands.handHud.compassDistanceMetres = v; },
	},
	{
		"HUD", "Hand panel height", "How far above the controller the hand HUD sits, metres",
		ItemKind::Number, -0.1f, 0.2f, 0.01f, 2, false,
		"HandHud", "PanelUp",
		+[](const Config& c) { return c.hands.handHud.panelUp; },
		+[](Config& c, float v) { c.hands.handHud.panelUp = v; },
	},
	{
		"HUD", "Hand panel back", "How far back along the controller it sits, metres",
		ItemKind::Number, -0.1f, 0.3f, 0.01f, 2, false,
		"HandHud", "PanelBack",
		+[](const Config& c) { return c.hands.handHud.panelBack; },
		+[](Config& c, float v) { c.hands.handHud.panelBack = v; },
	},
	{
		"HUD", "Hand panel tilt", "How far it leans towards your eyes, degrees",
		ItemKind::Number, 0.0f, 90.0f, 5.0f, 0, false,
		"HandHud", "PanelTiltDegrees",
		+[](const Config& c) { return c.hands.handHud.panelTiltDegrees; },
		+[](Config& c, float v) { c.hands.handHud.panelTiltDegrees = v; },
	},
	{
		"HUD", "Enemy health over them",
		"Every fighter attacking you carries the game's own health bar (its golden arc) above their head in the "
		"world; the one at the crosshair is hidden",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"HandHud", "EnemyHealthBars",
		+[](const Config& c) { return c.hands.enemyHealth.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.enemyHealth.enabled = v != 0.0f; },
	},
	{
		"HUD", "Enemy health range (m)", "Fighters farther away than this get no bar",
		ItemKind::Number, 1.0f, 100.0f, 1.0f, 0, false,
		"HandHud", "EnemyHealthMetres",
		+[](const Config& c) { return c.hands.enemyHealth.rangeMetres; },
		+[](Config& c, float v) { c.hands.enemyHealth.rangeMetres = v; },
	},
	{
		"HUD", "Enemy health bar width (m)",
		"How wide the bar is in the world up close (the arc takes about two thirds of it); far off it keeps a "
		"minimum size",
		ItemKind::Number, 0.1f, 2.0f, 0.05f, 2, false,
		"HandHud", "EnemyHealthWidthMetres",
		+[](const Config& c) { return c.hands.enemyHealth.widthMetres; },
		+[](Config& c, float v) { c.hands.enemyHealth.widthMetres = v; },
	},
	{
		"HUD", "Enemy health height (m)", "How far above the fighter's head bone the bar's centre hangs",
		ItemKind::Number, 0.05f, 1.0f, 0.05f, 2, false,
		"HandHud", "EnemyHealthAboveMetres",
		+[](const Config& c) { return c.hands.enemyHealth.aboveMetres; },
		+[](Config& c, float v) { c.hands.enemyHealth.aboveMetres = v; },
	},
	{
		"HUD", "Enemy health shown (s)",
		"A bar shows while you see a hurt fighter, this long after their health last moved, then fades out",
		ItemKind::Number, 1.0f, 60.0f, 1.0f, 0, false,
		"HandHud", "EnemyHealthShowSeconds",
		+[](const Config& c) { return c.hands.enemyHealth.showSeconds; },
		+[](Config& c, float v) { c.hands.enemyHealth.showSeconds = v; },
	},
	// ---- Teleport (Full VR) --------------------------------------------------
	{
		"Teleport", "Teleport", "Full VR: hold the right stick up, aim the arc, let go",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Locomotion", "Teleport",
		+[](const Config& c) { return c.hands.teleport.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.teleport.enabled = v != 0.0f; },
	},
	{
		"Teleport", "Instant", "Fade to black and back instead of gliding there",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Locomotion", "TeleportInstant",
		+[](const Config& c) { return c.hands.teleport.instant ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.teleport.instant = v != 0.0f; },
	},
	{
		"Teleport", "Range", "How far a teleport reaches, metres",
		ItemKind::Number, 1.0f, 8.0f, 0.5f, 1, false,
		"Locomotion", "TeleportRangeMetres",
		+[](const Config& c) { return c.hands.teleport.rangeMetres; },
		+[](Config& c, float v) { c.hands.teleport.rangeMetres = v; },
	},
	{
		"Teleport", "Glide speed", "How fast the glide carries you, metres a second",
		ItemKind::Number, vr::kGlideSpeedMin, vr::kGlideSpeedMax, 1.0f, 0, false,
		"Locomotion", "TeleportGlideSpeed",
		+[](const Config& c) { return c.hands.teleport.glideMetresPerSecond; },
		+[](Config& c, float v) { c.hands.teleport.glideMetresPerSecond = v; },
	},
	{
		"Teleport", "Fade time", "Instant mode: seconds to black, and as long back",
		ItemKind::Number, 0.05f, 0.5f, 0.05f, 2, false,
		"Locomotion", "TeleportFadeSeconds",
		+[](const Config& c) { return c.hands.teleport.fadeSeconds; },
		+[](Config& c, float v) { c.hands.teleport.fadeSeconds = v; },
	},
	{
		"Teleport", "Instant: fade to black", "Off: the instant teleport jumps there with no black",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Locomotion", "TeleportInstantFade",
		+[](const Config& c) { return c.hands.teleport.instantFade ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.teleport.instantFade = v != 0.0f; },
	},
	{
		"Teleport", "In combat", "Teleport while fighting, as a dodge",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Locomotion", "TeleportInCombat",
		+[](const Config& c) { return c.hands.teleport.inCombat ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.teleport.inCombat = v != 0.0f; },
	},
	{
		"Teleport", "Vignette", "Darken the edges while gliding",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Locomotion", "TeleportVignette",
		+[](const Config& c) { return c.hands.teleport.vignette ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.teleport.vignette = v != 0.0f; },
	},
	{
		"Teleport", "Blink", "Also reach places walking cannot: roofs, ledges, across gaps",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Locomotion", "TeleportBlink",
		+[](const Config& c) { return c.hands.teleport.blink ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.teleport.blink = v != 0.0f; },
	},
	{
		"Teleport", "Makes noise", "Heard like walking while it moves; off, it is silent",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Locomotion", "TeleportMakesNoise",
		+[](const Config& c) { return c.hands.teleport.makesNoise ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.teleport.makesNoise = v != 0.0f; },
	},
	{
		"Teleport", "Sound", "With Makes noise, what the landing sounds like: the dodge roll, backwards, your steps, a swish",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kTeleportSoundCount - 1), 1.0f, 0, false,
		"Locomotion", "TeleportSound",
		+[](const Config& c) { return static_cast<float>(c.hands.teleport.sound); },
		+[](Config& c, float v) { c.hands.teleport.sound = vr::TeleportSoundFromIndex(v); },
		"", "", SettingAction::None, vr::kTeleportSoundNames, vr::kTeleportSoundCount,
	},
	{
		"Teleport", "Fatigue cost", "Times the price of the game's dodge roll; 0 is free",
		ItemKind::Number, 0.0f, 3.0f, 0.1f, 1, false,
		"Locomotion", "TeleportFatigueMult",
		+[](const Config& c) { return c.hands.teleport.fatigueMult; },
		+[](Config& c, float v) { c.hands.teleport.fatigueMult = v; },
	},
	{
		"Teleport", "Blink climb cost", "With Blink: extra fatigue per metre up",
		ItemKind::Number, 0.0f, 50.0f, 1.0f, 0, false,
		"Locomotion", "TeleportBlinkFatiguePerMetreUp",
		+[](const Config& c) { return c.hands.teleport.blinkFatiguePerMetreUp; },
		+[](Config& c, float v) { c.hands.teleport.blinkFatiguePerMetreUp = v; },
	},
	{
		"Teleport", "Stick push", "How far forward the stick goes before the arc shows",
		ItemKind::Number, 0.5f, 0.95f, 0.05f, 2, false,
		"Locomotion", "TeleportStickStart",
		+[](const Config& c) { return c.hands.teleport.startThreshold; },
		+[](Config& c, float v) { c.hands.teleport.startThreshold = v; },
	},
	{
		"Teleport", "Stick release", "How far back the stick comes before the teleport goes",
		ItemKind::Number, 0.1f, 0.5f, 0.05f, 2, false,
		"Locomotion", "TeleportStickRelease",
		+[](const Config& c) { return c.hands.teleport.releaseThreshold; },
		+[](Config& c, float v) { c.hands.teleport.releaseThreshold = v; },
	},
	{
		"Teleport", "Stick cone", "How far off straight forward the push still counts, degrees",
		ItemKind::Number, 10.0f, 45.0f, 5.0f, 0, false,
		"Locomotion", "TeleportStickConeDegrees",
		+[](const Config& c) { return c.hands.teleport.coneDegrees; },
		+[](Config& c, float v) { c.hands.teleport.coneDegrees = v; },
	},
	{
		"Teleport", "Hold to aim", "Right stick up: a flick jumps, held this long it aims, seconds",
		ItemKind::Number, 0.1f, 0.6f, 0.05f, 2, false,
		"Locomotion", "TeleportHoldSeconds",
		+[](const Config& c) { return c.hands.teleport.holdSeconds; },
		+[](Config& c, float v) { c.hands.teleport.holdSeconds = v; },
	},
	{
		"Help", "Show the controls", "Full VR: the controls card, the weapon hand and the other hand, by role",
		ItemKind::Action, 0.0f, 0.0f, 0.0f, 0, false,
		"", "",
		+[](const Config&) { return 0.0f; },
		+[](Config&, float) {},
		"", "", SettingAction::ShowControls,
	},
	{
		"Help", "Start guide", "Full VR: the short guide at every start - recenter, which hand, the controls, comfort",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Onboarding", "StartGuide",
		+[](const Config& c) { return c.onboardingStartGuide ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.onboardingStartGuide = v != 0.0f; },
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
	item.valueNames = definition.valueNames;
	item.valueNameCount = definition.valueNameCount;
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

bool CanonicalDefaultValue(const SettingDefinition& definition, float& value) {
	if (definition.Read == nullptr) {
		return false;
	}

	const Config defaults;
	value = definition.Read(defaults);
	return true;
}

bool CanResetSetting(const SettingDefinition& definition, const Config& config) {
	if (definition.kind == ItemKind::Action || definition.kind == ItemKind::Text ||
	    definition.Read == nullptr || definition.Write == nullptr) {
		return false;
	}

	float defaultValue = 0.0f;
	return CanonicalDefaultValue(definition, defaultValue) &&
	       definition.Read(config) != defaultValue;
}

namespace {

bool KeyIs(const char* key, const char* wanted) {
	if (key == nullptr) {
		return false;
	}
	while (*key != '\0' && *wanted != '\0' && *key == *wanted) {
		++key;
		++wanted;
	}
	return *key == '\0' && *wanted == '\0';
}

}  // namespace

const char* SettingEditRefusal(const SettingDefinition& definition, const Config& config,
                               float value) {
	const bool off = value == 0.0f;
	if (off && KeyIs(definition.iniKey, "LaserBeam") && !config.hands.laserDot) {
		return "The laser needs the beam or the dot: switch the dot on first.";
	}
	if (off && KeyIs(definition.iniKey, "LaserDot") && !config.hands.laserBeam) {
		return "The laser needs the beam or the dot: switch the beam on first.";
	}
	return nullptr;
}

bool CategoryUnderHands(const char* category) {
	if (category == nullptr) {
		return false;
	}
	static const char kHands[] = "Hands";
	UInt32 at = 0;
	for (; kHands[at] != '\0'; ++at) {
		if (category[at] != kHands[at]) {
			return false;
		}
	}
	// "Hands" itself, or "Hands: <topic>" - never "Handsome" or "HandHud".
	return category[at] == '\0' || (category[at] == ':' && category[at + 1] == ' ');
}

}  // namespace obvr::ui
