#pragma once

// The settings menu's rows under HUD, Body & hands:
// one part of the table ui/SettingsList.cpp joins, in order (split 2026-11-11
// so the menu can grow under the size ratchet, tests/function-sizes.txt). The
// rows are the table's, moved unchanged; tests/settings-rows.txt pins them.

#include "ui/SettingsHudRows.h"
#include "ui/SettingsList.h"
#include "vr/HandSettingsOps.h"

namespace obvr::ui::settings_rows {

inline constexpr SettingDefinition kBodyRows[] = {
	// ---- HUD -----------------------------------------------------------------
	{
		"HUD", "Crosshair place", "laser: ahead of the pointing hand. target: over the thing pointed at, facing you. view: straight ahead",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kCrosshairPlaceCount - 1), 1.0f, 0, false,
		"Hands", "CrosshairPlace",
		+[](const Config& c) { return static_cast<float>(c.hands.crosshairPlace); },
		+[](Config& c, float v) { c.hands.crosshairPlace = vr::CrosshairPlaceFromIndex(v); },
		"", "", SettingAction::None, vr::kCrosshairPlaceNames, vr::kCrosshairPlaceCount,
	},
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
	OBVR_HUD_ELEMENT_ROWS(10, "Leveling text", "XpText")
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
	// ---- Body & hands --------------------------------------------------------

	{
		"Body & hands", "Your own body", "Look down and see a body under the headset",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "Visible",
		+[](const Config& c) { return c.body.visible ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.visible = v != 0.0f; },
	},
	{
		"Body & hands", "Follows you (IK)", "The body's arms reach your hands, it turns with you, it crouches with you",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IK",
		+[](const Config& c) { return c.body.ik ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ik = v != 0.0f; },
	},
	{
		"Body & hands", "Arms reach your hands", "Each arm of the body is bent to meet the hand you hold out",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKArms",
		+[](const Config& c) { return c.body.ikArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikArms = v != 0.0f; },
	},
	{
		"Body & hands", "Turns with your head", "The body stands steady under your head and follows where you look: a glance leaves it, a held look brings it round",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKTorso",
		+[](const Config& c) { return c.body.ikTorso ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikTorso = v != 0.0f; },
	},
	{
		"Body & hands", "Knees bend when you crouch", "Crouch for real and the body's knees bend, its feet stay on the ground",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKLegs",
		+[](const Config& c) { return c.body.ikLegs ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikLegs = v != 0.0f; },
	},
	{
		"Body & hands", "Bends at the hips", "Lean forward and the body bends at the hips, its feet staying where you stand",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKLean",
		+[](const Config& c) { return c.body.ikLean ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikLean = v != 0.0f; },
	},
	{
		"Body & hands", "Soft push", "Your hands press the body's BBB breasts in, they spring back when you let go",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "SoftPush",
		+[](const Config& c) { return c.body.softPush ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.softPush = v != 0.0f; },
	},
	{
		"Body & hands", "Soft push reach", "How near a palm presses, in game units (13 is about 19 cm)",
		ItemKind::Number, 2.0f, 20.0f, 1.0f, 0, false,
		"Body", "SoftPushReach",
		+[](const Config& c) { return c.body.softPushReachUnits; },
		+[](Config& c, float v) { c.body.softPushReachUnits = v; },
	},
	{
		"Body & hands", "Soft push depth", "How far the breasts give at most, in game units (6 is about 9 cm)",
		ItemKind::Number, 1.0f, 12.0f, 0.5f, 1, false,
		"Body", "SoftPushMost",
		+[](const Config& c) { return c.body.softPushMostUnits; },
		+[](Config& c, float v) { c.body.softPushMostUnits = v; },
	},
	{
		"Body & hands", "Head's free turn (deg)", "How far you can look aside before the body turns with you",
		ItemKind::Number, 0.0f, 90.0f, 5.0f, 0, false,
		"Body", "TorsoFollowDegrees",
		+[](const Config& c) { return c.body.torsoFollowDegrees; },
		+[](Config& c, float v) { c.body.torsoFollowDegrees = v; },
	},
	{
		"Body & hands", "Body follows after (s)", "How long a held look takes to bring the body round",
		ItemKind::Number, 0.0f, 5.0f, 0.1f, 1, false,
		"Body", "TorsoFollowSeconds",
		+[](const Config& c) { return c.body.torsoFollowSeconds; },
		+[](Config& c, float v) { c.body.torsoFollowSeconds = v; },
	},
	{
		"Body & hands", "Arm stretch", "How much longer the body's arms may grow to reach your hands",
		ItemKind::Number, 1.0f, 1.5f, 0.05f, 2, false,
		"Body", "ArmStretch",
		+[](const Config& c) { return c.body.armStretch; },
		+[](Config& c, float v) { c.body.armStretch = v; },
	},
	{
		"Body & hands", "Hide its arms", "Arms that do not follow your hands go; the first-person arms stay",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "HideArms",
		+[](const Config& c) { return c.body.hideArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.hideArms = v != 0.0f; },
	},
	{
		"Body & hands", "Eyes forward", "How far the eyes sit in front of the head bone, in units",
		ItemKind::Number, -20.0f, 40.0f, 0.2f, 1, false,
		"Body", "EyeForward",
		+[](const Config& c) { return c.body.eyeForward; },
		+[](Config& c, float v) { c.body.eyeForward = v; },
	},
	{
		"Body & hands", "Eyes up", "How far the eyes sit above the head bone, in units",
		ItemKind::Number, -20.0f, 30.0f, 1.0f, 0, false,
		"Body", "EyeUp",
		+[](const Config& c) { return c.body.eyeUp; },
		+[](Config& c, float v) { c.body.eyeUp = v; },
	},
	{
		"Body & hands", "Near plane", "Units from the eyes where the picture begins; nearer shows more chest looking down, 0 is the game's 10",
		ItemKind::Number, 0.0f, 10.0f, 0.5f, 1, false,
		"Body", "NearPlane",
		+[](const Config& c) { return c.body.nearPlaneUnits; },
		+[](Config& c, float v) { c.body.nearPlaneUnits = v; },
	},
	{
		"Body & hands", "Hands on the controllers", "The hand bones follow the controllers, not the animation",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "PinHands",
		+[](const Config& c) { return c.hands.pinHands ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.pinHands = v != 0.0f; },
	},
	{
		"Body & hands", "Finger tracking", "An empty hand's fingers follow yours on the controller",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "FingerTracking",
		+[](const Config& c) { return c.hands.fingerTracking ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.fingerTracking = v != 0.0f; },
	},
	{
		"Body & hands", "Hide the arms", "The animated arms go; hands and weapon stay",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "HideArms",
		+[](const Config& c) { return c.hands.hideArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.hideArms = v != 0.0f; },
	},
	{
		"Body & hands", "Closed hands", "The inside of the hands drawn, darker, not see-through at the wrist",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ClosedHands",
		+[](const Config& c) { return c.hands.closedHands ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.closedHands = v != 0.0f; },
	},
	{
		"Body & hands", "Inside brightness", "How bright the inside of the hands is: 0 black, 1 as the outside",
		ItemKind::Number, 0.0f, 1.0f, 0.1f, 1, false,
		"Hands", "ClosedHandsBrightness",
		+[](const Config& c) { return c.hands.closedHandsBrightness; },
		+[](Config& c, float v) { c.hands.closedHandsBrightness = v; },
	},
	{
		"Body & hands", "Close bare wrists", "A bare hand's wrist drawn closed, not open to look into",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "CloseBareWrists",
		+[](const Config& c) { return c.hands.closeBareWrists ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.closeBareWrists = v != 0.0f; },
	},
	{
		"Body & hands", "Hand height", "Moves both hands along the controller's up axis, metres; lower is negative",
		ItemKind::Number, -0.15f, 0.10f, 0.01f, 2, false,
		"Hands", "HandGripUpMetres",
		+[](const Config& c) { return c.hands.handGripUpMetres; },
		+[](Config& c, float v) { c.hands.handGripUpMetres = v; },
	},
	{
		"Body & hands", "Hand forward", "Moves both hands along the controller's forward axis, metres",
		ItemKind::Number, -0.15f, 0.15f, 0.01f, 2, false,
		"Hands", "HandGripForwardMetres",
		+[](const Config& c) { return c.hands.handGripForwardMetres; },
		+[](Config& c, float v) { c.hands.handGripForwardMetres = v; },
	},
	{
		"Body & hands", "Right hand roll", "Turns the right palm about the bone, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "RightHandRoll",
		+[](const Config& c) { return c.hands.rightHandRoll; },
		+[](Config& c, float v) { c.hands.rightHandRoll = v; },
	},
	{
		"Body & hands", "Right hand yaw", "Turns the right hand about the up axis, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "RightHandYaw",
		+[](const Config& c) { return c.hands.rightHandYaw; },
		+[](Config& c, float v) { c.hands.rightHandYaw = v; },
	},
	{
		"Body & hands", "Right hand pitch", "Tips the right hand up or down about its side axis, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "RightHandPitch",
		+[](const Config& c) { return c.hands.rightHandPitch; },
		+[](Config& c, float v) { c.hands.rightHandPitch = v; },
	},
	{
		"Body & hands", "Left hand roll", "Turns the left palm about the bone, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "LeftHandRoll",
		+[](const Config& c) { return c.hands.leftHandRoll; },
		+[](Config& c, float v) { c.hands.leftHandRoll = v; },
	},
	{
		"Body & hands", "Left hand yaw", "Turns the left hand about the up axis, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "LeftHandYaw",
		+[](const Config& c) { return c.hands.leftHandYaw; },
		+[](Config& c, float v) { c.hands.leftHandYaw = v; },
	},
	{
		"Body & hands", "Left hand pitch", "Tips the left hand up or down about its side axis, degrees",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "LeftHandPitch",
		+[](const Config& c) { return c.hands.leftHandPitch; },
		+[](Config& c, float v) { c.hands.leftHandPitch = v; },
	},
};

}  // namespace obvr::ui::settings_rows
