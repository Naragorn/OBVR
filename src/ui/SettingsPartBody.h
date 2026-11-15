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
		"HUD", "Crosshair position", "Put the crosshair on your laser, on what you point at, or straight ahead",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kCrosshairPlaceCount - 1), 1.0f, 0, false,
		"Hands", "CrosshairPlace",
		+[](const Config& c) { return static_cast<float>(c.hands.crosshairPlace); },
		+[](Config& c, float v) { c.hands.crosshairPlace = vr::CrosshairPlaceFromIndex(v); },
		"", "", SettingAction::None, vr::kCrosshairPlaceNames, vr::kCrosshairPlaceCount,
	},
	{
		"HUD", "HUD on the hands", "Spread your HUD across your hands, with the compass up in the sky",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"HandHud", "Enabled",
		+[](const Config& c) { return c.hands.handHud.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.handHud.enabled = v != 0.0f; },
	},
	{
		"HUD", "Show on a glance",
		"Only show the hand HUD when you look at your palm. Off, it's always there",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"HandHud", "ShowOnLook",
		+[](const Config& c) { return c.hands.handHud.showOnLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.handHud.showOnLook = v != 0.0f; },
	},
	{
		"HUD", "Glance angle", "How close to the middle of your view your hand needs to be",
		ItemKind::Number, 10.0f, 80.0f, 5.0f, 0, false,
		"HandHud", "LookGazeDegrees",
		+[](const Config& c) { return c.hands.handHud.lookGazeDegrees; },
		+[](Config& c, float v) { c.hands.handHud.lookGazeDegrees = v; },
	},
	{
		"HUD", "Palm angle", "How squarely your palm must face you",
		ItemKind::Number, 10.0f, 90.0f, 5.0f, 0, false,
		"HandHud", "LookPalmDegrees",
		+[](const Config& c) { return c.hands.handHud.lookPalmDegrees; },
		+[](Config& c, float v) { c.hands.handHud.lookPalmDegrees = v; },
	},
	{
		"HUD", "Glance fade", "How fast the hand HUD fades in and out, in seconds",
		ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false,
		"HandHud", "LookFadeSeconds",
		+[](const Config& c) { return c.hands.handHud.lookFadeSeconds; },
		+[](Config& c, float v) { c.hands.handHud.lookFadeSeconds = v; },
	},
	{
		"HUD", "HUD over the palm", "How far above your palm the HUD floats",
		ItemKind::Number, 0.0f, 0.2f, 0.01f, 2, false,
		"HandHud", "PalmLiftMetres",
		+[](const Config& c) { return c.hands.handHud.palmLiftMetres; },
		+[](Config& c, float v) { c.hands.handHud.palmLiftMetres = v; },
	},
	{
		"HUD", "HUD down the palm", "How far down your palm the HUD sits",
		ItemKind::Number, -0.1f, 0.2f, 0.01f, 2, false,
		"HandHud", "PalmDownMetres",
		+[](const Config& c) { return c.hands.handHud.palmDownMetres; },
		+[](Config& c, float v) { c.hands.handHud.palmDownMetres = v; },
	},
	{
		"HUD", "Hand HUD left/right", "Nudge the hand HUD left or right",
		ItemKind::Number, -0.15f, 0.15f, 0.01f, 2, false,
		"HandHud", "OffsetRightMetres",
		+[](const Config& c) { return c.hands.handHud.offsetRightMetres; },
		+[](Config& c, float v) { c.hands.handHud.offsetRightMetres = v; },
	},
	{
		"HUD", "Hand HUD up/down", "Nudge the hand HUD up or down",
		ItemKind::Number, -0.15f, 0.15f, 0.01f, 2, false,
		"HandHud", "OffsetUpMetres",
		+[](const Config& c) { return c.hands.handHud.offsetUpMetres; },
		+[](Config& c, float v) { c.hands.handHud.offsetUpMetres = v; },
	},
	OBVR_HUD_ELEMENT_ROWS(0, "Health bars", "Bars")
	OBVR_HUD_ELEMENT_ROWS(1, "Spell icon", "Spell")
	OBVR_HUD_ELEMENT_ROWS(2, "Weapon icon", "Weapon")
	OBVR_HUD_ELEMENT_ROWS(3, "Active effects", "Effects")
	OBVR_HUD_ELEMENT_ROWS(4, "Level-up icon", "LevelUp")
	OBVR_HUD_ELEMENT_ROWS(5, "Compass", "Compass")
	OBVR_HUD_ELEMENT_ROWS(6, "Region name", "Region")
	OBVR_HUD_ELEMENT_ROWS(7, "Messages", "Messages")
	OBVR_HUD_ELEMENT_ROWS(8, "Subtitles", "Subtitles")
	OBVR_HUD_ELEMENT_ROWS(9, "Target info", "Info")
	OBVR_HUD_ELEMENT_ROWS(10, "Leveling text", "XpText")
	{
		"HUD", "Text distance", "How far away messages and subtitles float",
		ItemKind::Number, 0.5f, 3.0f, 0.1f, 1, false,
		"HandHud", "ViewDistanceMetres",
		+[](const Config& c) { return c.hands.handHud.viewDistanceMetres; },
		+[](Config& c, float v) { c.hands.handHud.viewDistanceMetres = v; },
	},
	{
		"HUD", "Top row height", "How high above the middle of your view the top row sits",
		ItemKind::Number, 0.0f, 45.0f, 1.0f, 0, false,
		"HandHud", "ViewTopDegrees",
		+[](const Config& c) { return c.hands.handHud.viewTopDegrees; },
		+[](Config& c, float v) { c.hands.handHud.viewTopDegrees = v; },
	},
	{
		"HUD", "Bottom row height", "How far below the middle of your view the bottom row sits",
		ItemKind::Number, 0.0f, 45.0f, 1.0f, 0, false,
		"HandHud", "ViewBottomDegrees",
		+[](const Config& c) { return c.hands.handHud.viewBottomDegrees; },
		+[](Config& c, float v) { c.hands.handHud.viewBottomDegrees = v; },
	},
	{
		"HUD", "Text catches up after", "How far you turn before the text follows you. 0 never",
		ItemKind::Number, 0.0f, 90.0f, 5.0f, 0, false,
		"HandHud", "ViewFollowDegrees",
		+[](const Config& c) { return c.hands.handHud.viewFollowDegrees; },
		+[](Config& c, float v) { c.hands.handHud.viewFollowDegrees = v; },
	},
	{
		"HUD", "Text catch-up time", "How long the text takes to catch up. 0 is instant",
		ItemKind::Number, 0.0f, 2.0f, 0.1f, 1, false,
		"HandHud", "ViewFollowSeconds",
		+[](const Config& c) { return c.hands.handHud.viewFollowSeconds; },
		+[](Config& c, float v) { c.hands.handHud.viewFollowSeconds = v; },
	},
	{
		"HUD", "Text locked to the head", "Messages and subtitles move with your head",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"HandHud", "ViewLockedToHead",
		+[](const Config& c) { return c.hands.handHud.viewLockedToHead ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.handHud.viewLockedToHead = v != 0.0f; },
	},
	{
		"HUD", "Compass height", "How high in the sky the compass sits",
		ItemKind::Number, 15.0f, 85.0f, 5.0f, 0, false,
		"HandHud", "CompassElevationDegrees",
		+[](const Config& c) { return c.hands.handHud.compassElevationDegrees; },
		+[](Config& c, float v) { c.hands.handHud.compassElevationDegrees = v; },
	},
	{
		"HUD", "Compass shows from", "How far up you look before the compass appears",
		ItemKind::Number, 0.0f, 80.0f, 5.0f, 0, false,
		"HandHud", "CompassFadeStartDegrees",
		+[](const Config& c) { return c.hands.handHud.compassFadeStartDegrees; },
		+[](Config& c, float v) { c.hands.handHud.compassFadeStartDegrees = v; },
	},
	{
		"HUD", "Compass full from", "How far up you look before it's fully there",
		ItemKind::Number, 0.0f, 85.0f, 5.0f, 0, false,
		"HandHud", "CompassFadeFullDegrees",
		+[](const Config& c) { return c.hands.handHud.compassFadeFullDegrees; },
		+[](Config& c, float v) { c.hands.handHud.compassFadeFullDegrees = v; },
	},
	{
		"HUD", "Compass distance", "How far away the compass floats",
		ItemKind::Number, 0.5f, 5.0f, 0.1f, 1, false,
		"HandHud", "CompassDistanceMetres",
		+[](const Config& c) { return c.hands.handHud.compassDistanceMetres; },
		+[](Config& c, float v) { c.hands.handHud.compassDistanceMetres = v; },
	},
	{
		"HUD", "Hand panel height", "How far above the controller the hand HUD sits",
		ItemKind::Number, -0.1f, 0.2f, 0.01f, 2, false,
		"HandHud", "PanelUp",
		+[](const Config& c) { return c.hands.handHud.panelUp; },
		+[](Config& c, float v) { c.hands.handHud.panelUp = v; },
	},
	{
		"HUD", "Hand panel back", "How far back along the controller it sits",
		ItemKind::Number, -0.1f, 0.3f, 0.01f, 2, false,
		"HandHud", "PanelBack",
		+[](const Config& c) { return c.hands.handHud.panelBack; },
		+[](Config& c, float v) { c.hands.handHud.panelBack = v; },
	},
	{
		"HUD", "Hand panel tilt", "How far it tilts toward your eyes",
		ItemKind::Number, 0.0f, 90.0f, 5.0f, 0, false,
		"HandHud", "PanelTiltDegrees",
		+[](const Config& c) { return c.hands.handHud.panelTiltDegrees; },
		+[](Config& c, float v) { c.hands.handHud.panelTiltDegrees = v; },
	},
	{
		"HUD", "Enemy health bars",
		"Show a health bar over every foe fighting you",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"HandHud", "EnemyHealthBars",
		+[](const Config& c) { return c.hands.enemyHealth.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.enemyHealth.enabled = v != 0.0f; },
	},
	{
		"HUD", "Enemy bar range", "Foes farther away than this get no bar, in metres",
		ItemKind::Number, 1.0f, 100.0f, 1.0f, 0, false,
		"HandHud", "EnemyHealthMetres",
		+[](const Config& c) { return c.hands.enemyHealth.rangeMetres; },
		+[](Config& c, float v) { c.hands.enemyHealth.rangeMetres = v; },
	},
	{
		"HUD", "Enemy bar width",
		"How wide the bar is up close",
		ItemKind::Number, 0.1f, 2.0f, 0.05f, 2, false,
		"HandHud", "EnemyHealthWidthMetres",
		+[](const Config& c) { return c.hands.enemyHealth.widthMetres; },
		+[](Config& c, float v) { c.hands.enemyHealth.widthMetres = v; },
	},
	{
		"HUD", "Enemy bar height", "How high above their head the bar floats",
		ItemKind::Number, 0.05f, 1.0f, 0.05f, 2, false,
		"HandHud", "EnemyHealthAboveMetres",
		+[](const Config& c) { return c.hands.enemyHealth.aboveMetres; },
		+[](Config& c, float v) { c.hands.enemyHealth.aboveMetres = v; },
	},
	{
		"HUD", "Enemy bar time",
		"How long the bar stays after their health last changed, in seconds",
		ItemKind::Number, 1.0f, 60.0f, 1.0f, 0, false,
		"HandHud", "EnemyHealthShowSeconds",
		+[](const Config& c) { return c.hands.enemyHealth.showSeconds; },
		+[](Config& c, float v) { c.hands.enemyHealth.showSeconds = v; },
	},
	// ---- Body & hands --------------------------------------------------------

	{
		"Body & hands", "Your own body", "Look down and see your body",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "Visible",
		+[](const Config& c) { return c.body.visible ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.visible = v != 0.0f; },
	},
	{
		"Body & hands", "Body follows you", "Your body reaches for your hands and turns and crouches with you",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IK",
		+[](const Config& c) { return c.body.ik ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ik = v != 0.0f; },
	},
	{
		"Body & hands", "Arms reach your hands", "Your body's arms bend to meet your hands",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKArms",
		+[](const Config& c) { return c.body.ikArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikArms = v != 0.0f; },
	},
	{
		"Body & hands", "Body turns with you", "A quick glance leaves your body be. A longer look turns it",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKTorso",
		+[](const Config& c) { return c.body.ikTorso ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikTorso = v != 0.0f; },
	},
	{
		"Body & hands", "Knees bend when you crouch", "Crouch for real and your knees bend too",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKLegs",
		+[](const Config& c) { return c.body.ikLegs ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikLegs = v != 0.0f; },
	},
	{
		"Body & hands", "Bends at the hips", "Lean forward and your body bends at the hips",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "IKLean",
		+[](const Config& c) { return c.body.ikLean ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.ikLean = v != 0.0f; },
	},
	{
		"Body & hands", "Soft push", "With a BBB body, your hands press breasts in and they spring back",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "SoftPush",
		+[](const Config& c) { return c.body.softPush ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.softPush = v != 0.0f; },
	},
	{
		"Body & hands", "Soft push reach", "How close your palm must be to press",
		ItemKind::Number, 2.0f, 20.0f, 1.0f, 0, false,
		"Body", "SoftPushReach",
		+[](const Config& c) { return c.body.softPushReachUnits; },
		+[](Config& c, float v) { c.body.softPushReachUnits = v; },
	},
	{
		"Body & hands", "Soft push depth", "How far they give at most",
		ItemKind::Number, 1.0f, 12.0f, 0.5f, 1, false,
		"Body", "SoftPushMost",
		+[](const Config& c) { return c.body.softPushMostUnits; },
		+[](Config& c, float v) { c.body.softPushMostUnits = v; },
	},
	{
		"Body & hands", "Look before body turns", "How far you can look aside before your body follows",
		ItemKind::Number, 0.0f, 90.0f, 5.0f, 0, false,
		"Body", "TorsoFollowDegrees",
		+[](const Config& c) { return c.body.torsoFollowDegrees; },
		+[](Config& c, float v) { c.body.torsoFollowDegrees = v; },
	},
	{
		"Body & hands", "Body turn delay", "How long you look aside before your body turns, in seconds",
		ItemKind::Number, 0.0f, 5.0f, 0.1f, 1, false,
		"Body", "TorsoFollowSeconds",
		+[](const Config& c) { return c.body.torsoFollowSeconds; },
		+[](Config& c, float v) { c.body.torsoFollowSeconds = v; },
	},
	{
		"Body & hands", "Arm stretch", "How much your body's arms can stretch to reach your hands",
		ItemKind::Number, 1.0f, 1.5f, 0.05f, 2, false,
		"Body", "ArmStretch",
		+[](const Config& c) { return c.body.armStretch; },
		+[](Config& c, float v) { c.body.armStretch = v; },
	},
	{
		"Body & hands", "Hide body arms", "Hide your body's arms when they can't follow your hands",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Body", "HideArms",
		+[](const Config& c) { return c.body.hideArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.body.hideArms = v != 0.0f; },
	},
	{
		"Body & hands", "Eyes forward", "Move your view forward or back inside your head",
		ItemKind::Number, -20.0f, 40.0f, 0.2f, 1, false,
		"Body", "EyeForward",
		+[](const Config& c) { return c.body.eyeForward; },
		+[](Config& c, float v) { c.body.eyeForward = v; },
	},
	{
		"Body & hands", "Eyes up", "Move your view up or down inside your head",
		ItemKind::Number, -20.0f, 30.0f, 1.0f, 0, false,
		"Body", "EyeUp",
		+[](const Config& c) { return c.body.eyeUp; },
		+[](Config& c, float v) { c.body.eyeUp = v; },
	},
	{
		"Body & hands", "Close-up clipping", "Lower lets you see more of your chest when you look down. 0 is normal",
		ItemKind::Number, 0.0f, 10.0f, 0.5f, 1, false,
		"Body", "NearPlane",
		+[](const Config& c) { return c.body.nearPlaneUnits; },
		+[](Config& c, float v) { c.body.nearPlaneUnits = v; },
	},
	{
		"Body & hands", "Hands on the controllers", "Your in-game hands follow your real ones, not the animation",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "PinHands",
		+[](const Config& c) { return c.hands.pinHands ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.pinHands = v != 0.0f; },
	},
	{
		"Body & hands", "Finger tracking", "An empty hand's fingers move with yours",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "FingerTracking",
		+[](const Config& c) { return c.hands.fingerTracking ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.fingerTracking = v != 0.0f; },
	},
	{
		"Body & hands", "Hide first-person arms", "Show only your hands and weapon, no arms",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "HideArms",
		+[](const Config& c) { return c.hands.hideArms ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.hideArms = v != 0.0f; },
	},
	{
		"Body & hands", "Fill in the hands", "Draw the inside of your hands so you can't see through the wrist",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "ClosedHands",
		+[](const Config& c) { return c.hands.closedHands ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.closedHands = v != 0.0f; },
	},
	{
		"Body & hands", "Hand inside brightness", "How bright the inside of your hands is. 0 is black",
		ItemKind::Number, 0.0f, 1.0f, 0.1f, 1, false,
		"Hands", "ClosedHandsBrightness",
		+[](const Config& c) { return c.hands.closedHandsBrightness; },
		+[](Config& c, float v) { c.hands.closedHandsBrightness = v; },
	},
	{
		"Body & hands", "Close bare wrists", "Close off bare wrists so you can't see inside",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "CloseBareWrists",
		+[](const Config& c) { return c.hands.closeBareWrists ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.closeBareWrists = v != 0.0f; },
	},
	{
		"Body & hands", "Hand height", "Move both hands up or down on the controllers",
		ItemKind::Number, -0.15f, 0.10f, 0.01f, 2, false,
		"Hands", "HandGripUpMetres",
		+[](const Config& c) { return c.hands.handGripUpMetres; },
		+[](Config& c, float v) { c.hands.handGripUpMetres = v; },
	},
	{
		"Body & hands", "Hand forward", "Move both hands forward or back on the controllers",
		ItemKind::Number, -0.15f, 0.15f, 0.01f, 2, false,
		"Hands", "HandGripForwardMetres",
		+[](const Config& c) { return c.hands.handGripForwardMetres; },
		+[](Config& c, float v) { c.hands.handGripForwardMetres = v; },
	},
	{
		"Body & hands", "Right hand twist", "Twist your right hand around your wrist",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "RightHandRoll",
		+[](const Config& c) { return c.hands.rightHandRoll; },
		+[](Config& c, float v) { c.hands.rightHandRoll = v; },
	},
	{
		"Body & hands", "Right hand turn", "Turn your right hand left or right",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "RightHandYaw",
		+[](const Config& c) { return c.hands.rightHandYaw; },
		+[](Config& c, float v) { c.hands.rightHandYaw = v; },
	},
	{
		"Body & hands", "Right hand tip", "Tip your right hand up or down",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "RightHandPitch",
		+[](const Config& c) { return c.hands.rightHandPitch; },
		+[](Config& c, float v) { c.hands.rightHandPitch = v; },
	},
	{
		"Body & hands", "Left hand twist", "Twist your left hand around your wrist",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "LeftHandRoll",
		+[](const Config& c) { return c.hands.leftHandRoll; },
		+[](Config& c, float v) { c.hands.leftHandRoll = v; },
	},
	{
		"Body & hands", "Left hand turn", "Turn your left hand left or right",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "LeftHandYaw",
		+[](const Config& c) { return c.hands.leftHandYaw; },
		+[](Config& c, float v) { c.hands.leftHandYaw = v; },
	},
	{
		"Body & hands", "Left hand tip", "Tip your left hand up or down",
		ItemKind::Number, -180.0f, 180.0f, 5.0f, 0, false,
		"Hands", "LeftHandPitch",
		+[](const Config& c) { return c.hands.leftHandPitch; },
		+[](Config& c, float v) { c.hands.leftHandPitch = v; },
	},
};

}  // namespace obvr::ui::settings_rows
