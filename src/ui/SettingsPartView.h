#pragma once

// The settings menu's rows under Comfort, Looking, Aiming, Dialogue, Screen & menus, Picture & performance:
// one part of the table ui/SettingsList.cpp joins, in order (split 2026-11-11
// so the menu can grow under the size ratchet, tests/function-sizes.txt). The
// rows are the table's, moved unchanged; tests/settings-rows.txt pins them.

#include "ui/SettingsHudRows.h"
#include "ui/SettingsList.h"
#include "vr/HandSettingsOps.h"

namespace obvr::ui::settings_rows {

inline constexpr SettingDefinition kViewRows[] = {
	// ---- Comfort -------------------------------------------------------------
	{
		"Comfort", "Head movement", "How much leaning in your room moves you in the game",
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
		"Comfort", "Brown death tint", "Fade to the menus' brown instead of grey when you die",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DeathMenuTint",
		+[](const Config& c) { return c.look.deathMenuTint ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.deathMenuTint = v != 0.0f; },
	},
	{
		"Comfort", "See your body after death", "Before you reload, the view moves to your fallen body",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DeathBodyView",
		+[](const Config& c) { return c.look.deathBodyView ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.deathBodyView = v != 0.0f; },
	},
	{
		"Comfort", "Death view step back", "How far behind you the death view stands",
		ItemKind::Number, 0.0f, 2.0f, 0.1f, 1, false,
		"Look", "DeathViewBackMetres",
		+[](const Config& c) { return c.look.deathViewBackMetres; },
		+[](Config& c, float v) { c.look.deathViewBackMetres = v; },
	},
	{
		"Comfort", "Death body ahead", "How far in front of you your body falls",
		ItemKind::Number, 0.0f, 2.0f, 0.1f, 1, false,
		"Look", "DeathBodyAheadMetres",
		+[](const Config& c) { return c.look.deathBodyAheadMetres; },
		+[](Config& c, float v) { c.look.deathBodyAheadMetres = v; },
	},
	{
		"Comfort", "Death body height", "Raise or lower your fallen body",
		ItemKind::Number, -1.0f, 1.0f, 0.05f, 2, false,
		"Look", "DeathBodyUpMetres",
		+[](const Config& c) { return c.look.deathBodyUpMetres; },
		+[](Config& c, float v) { c.look.deathBodyUpMetres = v; },
	},
	{
		"Comfort", "Hide HUD when dead", "Hide the HUD and crosshair when you die",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "HideHudWhenDead",
		+[](const Config& c) { return c.look.hideHudWhenDead ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.hideHudWhenDead = v != 0.0f; },
	},
	{
		"Comfort", "Full-view fades", "Fades to black cover your whole view, not just a screen",
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
		"Comfort", "No knockdown", "Hits and spells never throw you around",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "NoPlayerKnockdown",
		+[](const Config& c) { return c.look.noPlayerKnockdown ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.noPlayerKnockdown = v != 0.0f; },
	},
	{
		"Comfort", "Exhaustion, not collapse",
		"Out of fatigue or paralysed, you stay up, but your weapon gets heavy and you gasp",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "Exhaustion",
		+[](const Config& c) { return c.look.exhaustion.enabled ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.exhaustion.enabled = v != 0.0f; },
	},
	{
		"Comfort", "Exhausted below (%)", "Below this much fatigue you're exhausted, in percent",
		ItemKind::Number, 1.0f, 50.0f, 1.0f, 0, false,
		"Look", "ExhaustionBelowPercent",
		+[](const Config& c) { return c.look.exhaustion.belowPercent; },
		+[](Config& c, float v) { c.look.exhaustion.belowPercent = v; },
	},
	{
		"Comfort", "Recovered above (%)", "Above this much fatigue you recover, in percent",
		ItemKind::Number, 1.0f, 90.0f, 1.0f, 0, false,
		"Look", "ExhaustionAbovePercent",
		+[](const Config& c) { return c.look.exhaustion.abovePercent; },
		+[](Config& c, float v) { c.look.exhaustion.abovePercent = v; },
	},
	{
		"Comfort", "Heft when exhausted", "How much heavier what you hold feels when you're exhausted",
		ItemKind::Number, 1.0f, 10.0f, 0.5f, 1, false,
		"Look", "ExhaustionWeight",
		+[](const Config& c) { return c.look.exhaustion.weight; },
		+[](Config& c, float v) { c.look.exhaustion.weight = v; },
	},
	{
		"Comfort", "Heavy weapon when tired", "Your weapon gets heavier when you're out of breath",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "ExhaustionHeavyWeapon",
		+[](const Config& c) { return c.look.exhaustion.heavyWeapon ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.exhaustion.heavyWeapon = v != 0.0f; },
	},
	{
		"Comfort", "Exhaustion sounds",
		"Gasp and pant while you're exhausted",
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
		"Comfort", "Idle camera spin", "The camera circles you when you stand idle. Best left off in VR",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "AutoVanityCamera",
		+[](const Config& c) { return c.look.autoVanityCamera ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.autoVanityCamera = v != 0.0f; },
	},
	{
		"Comfort", "Horse bob", "How much you bounce in the saddle. 0 is a steady ride",
		ItemKind::Number, 0.0f, 1.0f, 0.1f, 1, false,
		"Look", "HorseBob",
		+[](const Config& c) { return c.look.horseBob; },
		+[](Config& c, float v) { c.look.horseBob = v; },
	},
	{
		"Comfort", "Race menu face-to-face", "Look at your character head-on in the race menu. Off, the view stays put",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "ChargenView",
		+[](const Config& c) { return c.look.chargenView ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.chargenView = v != 0.0f; },
	},
	{
		"Comfort", "Race menu distance", "How far from the face you stand in the race menu",
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
		"Comfort", "Room tracking", "Leaning in your room moves you in the game",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Head", "PositionalTracking",
		+[](const Config& c) { return c.tracker.positionalTracking ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.positionalTracking = v != 0.0f; },
	},
	{
		"Comfort", "Real height", "See from your real eye height. Turn it off when you play seated",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Head", "RealHeight",
		+[](const Config& c) { return c.tracker.realHeight ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.realHeight = v != 0.0f; },
	},
	{
		"Comfort", "Lean limit", "How far you can lean before the view stops following",
		ItemKind::Number, 0.0f, 400.0f, 10.0f, 0, false,
		"Head", "MaxLeanUnits",
		+[](const Config& c) { return c.tracker.maxOffsetUnits; },
		+[](Config& c, float v) { c.tracker.maxOffsetUnits = v; },
	},
	{
		"Comfort", "First person after a load", "Loading a third-person save puts you back in first person",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "FirstPersonAfterLoad",
		+[](const Config& c) { return c.hands.firstPersonAfterLoad ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.firstPersonAfterLoad = v != 0.0f; },
	},
	{
		"Comfort", "First person after a knockdown", "Get knocked down and you're back in first person once you're up",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "FirstPersonAfterKnockdown",
		+[](const Config& c) { return c.hands.firstPersonAfterKnockdown ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.firstPersonAfterKnockdown = v != 0.0f; },
	},
	{
		"Comfort", "Animations in first person",
		"Sitting, falling, mounting and feeding play from your eyes. Off, from behind you",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "FirstPersonAnimations",
		+[](const Config& c) { return c.look.firstPersonAnimations ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.firstPersonAnimations = v != 0.0f; },
	},
	{
		"Comfort", "Animation vignette", "Darken the edges during those animations",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "AnimationVignette",
		+[](const Config& c) { return c.look.animationVignette ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.animationVignette = v != 0.0f; },
	},
	{
		"Comfort", "Animation vignette darkness", "How dark the edges get",
		ItemKind::Number, 0.0f, 1.0f, 0.1f, 1, false,
		"Look", "AnimationVignetteStrength",
		+[](const Config& c) { return c.look.animationVignetteStrength; },
		+[](Config& c, float v) { c.look.animationVignetteStrength = v; },
	},
	// ---- Looking -------------------------------------------------------------

	{
		"Looking", "Head look", "Look around with your head. Off, only the mouse or stick moves the view",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Head", "HeadLook",
		+[](const Config& c) { return c.tracker.headLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.headLook = v != 0.0f; },
	},
	{
		"Looking", "Aim by mouse, look by head", "The mouse or stick aims, your head just looks around",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "HeadTurnsViewOnly",
		+[](const Config& c) { return c.headTurnsViewOnly ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.headTurnsViewOnly = v != 0.0f; },
	},
	{
		"Looking", "Look up and down by head", "Your head looks up and down, not the mouse",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "BlockVerticalLook",
		+[](const Config& c) { return c.look.blockVerticalLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.blockVerticalLook = v != 0.0f; },
	},
	// ---- Aiming --------------------------------------------------------------

	{
		"Aiming", "Aim follows gaze", "Shots fly where you look",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "AimFollowsGaze",
		+[](const Config& c) { return c.aimFollowsGaze ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.aimFollowsGaze = v != 0.0f; },
	},
	{
		"Aiming", "Aim in third person", "Aim with your head in third person too",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "AimInThirdPerson",
		+[](const Config& c) { return c.aimInThirdPerson ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.aimInThirdPerson = v != 0.0f; },
	},
	{
		"Aiming", "Crosshair", "Show the crosshair in the headset",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "Crosshair",
		+[](const Config& c) { return c.tracker.crosshair ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshair = v != 0.0f; },
	},
	{
		"Aiming", "Crosshair depth", "The crosshair sits on what you aim at",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "CrosshairDynamic",
		+[](const Config& c) { return c.tracker.crosshairDynamic ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshairDynamic = v != 0.0f; },
	},
	{
		"Aiming", "Crosshair distance", "How far away it floats when you aim at nothing",
		ItemKind::Number, 0.5f, 30.0f, 0.5f, 1, false,
		"Render", "CrosshairDistanceMetres",
		+[](const Config& c) { return c.tracker.crosshairDistanceMetres; },
		+[](Config& c, float v) { c.tracker.crosshairDistanceMetres = v; },
	},
	{
		"Aiming", "Crosshair depth speed", "How fast the crosshair moves to a new distance. 0 is instant",
		ItemKind::Number, 0.0f, 30.0f, 1.0f, 0, false,
		"Render", "CrosshairDepthSpeed",
		+[](const Config& c) { return c.tracker.crosshairDepthSpeed; },
		+[](Config& c, float v) { c.tracker.crosshairDepthSpeed = v; },
	},
	{
		"Aiming", "Crosshair size", "How big the crosshair is",
		ItemKind::Number, 0.005f, 0.15f, 0.005f, 3, false,
		"Render", "CrosshairSizeAtOneMetre",
		+[](const Config& c) { return c.tracker.crosshairSizeAtOneMetre; },
		+[](Config& c, float v) { c.tracker.crosshairSizeAtOneMetre = v; },
	},
	{
		"Aiming", "Crosshair only when needed", "Hide it until you aim or can use something",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "CrosshairOnlyWhenNeeded",
		+[](const Config& c) { return c.tracker.crosshairOnlyWhenNeeded ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.crosshairOnlyWhenNeeded = v != 0.0f; },
	},
	{
		"Aiming", "Crosshair in 3rd person", "Show the crosshair in third person too",
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
	// ---- Dialogue ------------------------------------------------------------

	{
		"Dialogue", "Zoom in on faces", "Zoom in on whoever you talk to, like the original. Best left off in VR",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DialogZoom",
		+[](const Config& c) { return c.dialogZoom ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.dialogZoom = v != 0.0f; },
	},
	{
		"Dialogue", "First person in talks", "Switch to first person when a conversation starts",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DialogFirstPerson",
		+[](const Config& c) { return c.dialogFirstPerson ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.dialogFirstPerson = v != 0.0f; },
	},
	{
		"Dialogue", "Dialogue follows speaker", "The dialogue window moves to whoever you talk to",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "DialogRecentre",
		+[](const Config& c) { return c.dialogPanel.recentre ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.dialogPanel.recentre = v != 0.0f; },
	},
	{
		"Dialogue", "Dialogue window side", "In front of the speaker, or beside them so you can still see them",
		ItemKind::Number, 0.0f, static_cast<float>(vr::kDialogPanelSideCount - 1), 1.0f, 0, false,
		"Look", "DialogPanelSide",
		+[](const Config& c) { return static_cast<float>(c.dialogPanel.side); },
		+[](Config& c, float v) { c.dialogPanel.side = vr::DialogPanelSideFromIndex(v); },
		"", "", SettingAction::None, vr::kDialogPanelSideNames, vr::kDialogPanelSideCount,
	},
	{
		"Dialogue", "Dialogue window offset", "How far to the side of the speaker the window sits",
		ItemKind::Number, 5.0f, 60.0f, 5.0f, 0, false,
		"Look", "DialogPanelSideDegrees",
		+[](const Config& c) { return c.dialogPanel.sideDegrees; },
		+[](Config& c, float v) { c.dialogPanel.sideDegrees = v; },
	},
	{
		"Dialogue", "Dialogue window size", "How big the dialogue window is",
		ItemKind::Number, 0.4f, 1.5f, 0.05f, 2, false,
		"Look", "DialogPanelScale",
		+[](const Config& c) { return c.dialogPanel.scale; },
		+[](Config& c, float v) { c.dialogPanel.scale = v; },
	},
	// ---- Screen & menus ------------------------------------------------------

	{
		"Screen & menus", "HUD stays in place", "The HUD stays put in the room. Off, it moves with your head",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "HudAnchor",
		+[](const Config& c) { return c.tracker.hudAnchorWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.hudAnchorWorld = v != 0.0f; },
		"head", "world",
	},
	{
		"Screen & menus", "HUD distance", "How far away the HUD floats",
		ItemKind::Number, 0.4f, 8.0f, 0.1f, 1, false,
		"Render", "HudDistanceMetres",
		+[](const Config& c) { return c.tracker.hudDistanceMetres; },
		+[](Config& c, float v) { c.tracker.hudDistanceMetres = v; },
	},
	{
		"Screen & menus", "HUD width", "How wide the HUD is",
		ItemKind::Number, 0.4f, 8.0f, 0.1f, 1, false,
		"Render", "HudWidthMetres",
		+[](const Config& c) { return c.tracker.hudWidthMetres; },
		+[](Config& c, float v) { c.tracker.hudWidthMetres = v; },
	},
	{
		"Screen & menus", "Menu scale", "How big menus are",
		ItemKind::Number, 0.3f, 2.0f, 0.05f, 2, false,
		"Render", "MenuScale",
		+[](const Config& c) { return c.tracker.menuScale; },
		+[](Config& c, float v) { c.tracker.menuScale = v; },
	},
	{
		"Screen & menus", "Menus in the world", "Menus float in front of you instead of on a big screen",
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
		"Screen & menus", "Live world behind menus", "Behind the pause menu and journal, the world stays 3D as you look around",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "LiveMenuBackground",
		+[](const Config& c) { return c.tracker.liveMenuBackground ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.liveMenuBackground = v != 0.0f; },
	},
	{
		"Screen & menus", "Don't pause in menus", "The world keeps going while your inventory, map or spells are open",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "UnpausedMenus",
		+[](const Config& c) { return c.tracker.unpausedMenus ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.unpausedMenus = v != 0.0f; },
	},
	{
		"Screen & menus", "Loot window on the chest", "A container's window opens right over it, and the world keeps going",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "ContainerInWorld",
		+[](const Config& c) { return c.containerPanel.inWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.containerPanel.inWorld = v != 0.0f; },
	},
	{
		"Screen & menus", "Loot window size", "How big the loot window is",
		ItemKind::Number, 0.3f, 1.5f, 0.05f, 2, false,
		"Look", "ContainerPanelScale",
		+[](const Config& c) { return c.containerPanel.scale; },
		+[](Config& c, float v) { c.containerPanel.scale = v; },
	},
	{
		"Screen & menus", "Loot window height", "How high above the container the window sits",
		ItemKind::Number, -0.5f, 1.0f, 0.05f, 2, false,
		"Look", "ContainerPanelRaiseMetres",
		+[](const Config& c) { return c.containerPanel.raiseMetres; },
		+[](Config& c, float v) { c.containerPanel.raiseMetres = v; },
	},
	{
		"Screen & menus", "Big screen distance",
		"How far away the main menu, loading screens and videos look. 0 is very far",
		ItemKind::Number, 0.0f, 10.0f, 0.5f, 1, false,
		"Render", "FlatDepthMetres",
		+[](const Config& c) { return c.tracker.flatDepthMetres; },
		+[](Config& c, float v) { c.tracker.flatDepthMetres = v; },
	},
	{
		"Screen & menus", "Menu backdrop", "Tint the world brown behind menus, like the original",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "MenuShade",
		+[](const Config& c) { return c.tracker.menuShade ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.menuShade = v != 0.0f; },
	},
	{
		"Screen & menus", "Black loading screens", "Show black behind the loading art instead of the frozen world",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "LoadingBlack",
		+[](const Config& c) { return c.tracker.loadingBlack ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.loadingBlack = v != 0.0f; },
	},
	{
		"Screen & menus", "Loading sky",
		"What's around you while the game loads: Tamriel's night sky or a plain dark dome",
		ItemKind::Number, 0.0f, static_cast<float>(stage::kDomeLookCount - 1), 1.0f, 0, false,
		"Render", "LoadingStageLook",
		+[](const Config& c) { return static_cast<float>(c.loadingStageLook); },
		+[](Config& c, float v) { c.loadingStageLook = stage::DomeLookFromIndex(v); },
		"", "", SettingAction::None, stage::kDomeLookNames, stage::kDomeLookCount,
	},
	{
		"Screen & menus", "Menu backdrop strength", "How strong the tint is",
		ItemKind::Number, 0.0f, 1.0f, 0.05f, 2, false,
		"Render", "MenuShadeStrength",
		+[](const Config& c) { return c.tracker.menuShadeStrength; },
		+[](Config& c, float v) { c.tracker.menuShadeStrength = v; },
	},

	//
	// The settings menu's own placement, changeable from inside itself. Worth
	// having in reach: whether a panel hangs at the right distance is a question
	// nobody can answer except while looking at it.
	{
		"Screen & menus", "Settings stay in place", "This window stays put in the room. Off, it moves with your head",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"SettingsMenu", "Anchor",
		+[](const Config& c) { return c.settingsMenuInWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.settingsMenuInWorld = v != 0.0f; },
		"head", "world",
	},
	{
		"Screen & menus", "Settings distance", "How far away this window floats",
		ItemKind::Number, 0.3f, 4.0f, 0.05f, 2, false,
		"SettingsMenu", "DistanceMetres",
		+[](const Config& c) { return c.settingsMenuDistanceMetres; },
		+[](Config& c, float v) { c.settingsMenuDistanceMetres = v; },
	},
	{
		"Screen & menus", "Settings size", "How big this window is",
		ItemKind::Number, 0.3f, 4.0f, 0.05f, 2, false,
		"SettingsMenu", "WidthMetres",
		+[](const Config& c) { return c.settingsMenuWidthMetres; },
		+[](Config& c, float v) { c.settingsMenuWidthMetres = v; },
	},
	// ---- Picture & performance -----------------------------------------------

	//
	// Kept last and marked. These are read while the device is being created,
	// so changing one here has no effect until the game starts again - and
	// somebody who changes a setting and sees nothing happen concludes the menu
	// does not work, not that this particular row is different.
	{
		"Picture & performance", "Grass distance", "How far out grass grows. 0 is normal",
		ItemKind::Number, 0.0f, 8000.0f, 500.0f, 0, false,
		"Render", "GrassDistance",
		+[](const Config& c) { return c.grassDistance; },
		+[](Config& c, float v) { c.grassDistance = v; },
	},
	{
		"Picture & performance", "Grass density (%)", "How thick the grass is, in percent. 0 is normal",
		ItemKind::Number, 0.0f, 400.0f, 25.0f, 0, true,
		"Render", "GrassDensity",
		+[](const Config& c) { return c.grassDensity; },
		+[](Config& c, float v) { c.grassDensity = v; },
	},
	{
		"Picture & performance", "Sharpness, width", "How sharp the picture is side to side. 0 uses your headset's setting",
		ItemKind::Number, 0.0f, 8192.0f, 64.0f, 0, true,
		"Render", "GameResolutionWidth",
		+[](const Config& c) { return static_cast<float>(c.tracker.renderWidth); },
		+[](Config& c, float v) { c.tracker.renderWidth = static_cast<UInt32>(v); },
	},
	{
		"Picture & performance", "Sharpness, height", "How sharp the picture is top to bottom. 0 uses your headset's setting",
		ItemKind::Number, 0.0f, 8192.0f, 64.0f, 0, true,
		"Render", "GameResolutionHeight",
		+[](const Config& c) { return static_cast<float>(c.tracker.renderHeight); },
		+[](Config& c, float v) { c.tracker.renderHeight = static_cast<UInt32>(v); },
	},
	{
		"Picture & performance", "Faster stereo", "Draws both eyes at once for more speed. Needs OBNG",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, true,
		"Render", "SinglePassStereo",
		+[](const Config& c) { return c.singlePassStereo ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.singlePassStereo = v != 0.0f; },
	},
};

}  // namespace obvr::ui::settings_rows
