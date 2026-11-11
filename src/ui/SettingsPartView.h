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
		"Comfort", "Room tracking", "Leaning in the room moves the camera",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Head", "PositionalTracking",
		+[](const Config& c) { return c.tracker.positionalTracking ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.positionalTracking = v != 0.0f; },
	},
	{
		"Comfort", "View at your own height", "The view stands as high as your eyes over the room's floor, the body scaled to it; off for playing seated",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Head", "RealHeight",
		+[](const Config& c) { return c.tracker.realHeight ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.realHeight = v != 0.0f; },
	},
	{
		"Comfort", "Lean limit", "Oblivion units the camera may leave the head at",
		ItemKind::Number, 0.0f, 400.0f, 10.0f, 0, false,
		"Head", "MaxLeanUnits",
		+[](const Config& c) { return c.tracker.maxOffsetUnits; },
		+[](Config& c, float v) { c.tracker.maxOffsetUnits = v; },
	},
	{
		"Comfort", "First person after a load", "A save made in third person is put into first person once after it loads; a view chosen later stays",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "FirstPersonAfterLoad",
		+[](const Config& c) { return c.hands.firstPersonAfterLoad ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.firstPersonAfterLoad = v != 0.0f; },
	},
	{
		"Comfort", "First person after a knockdown", "Knocked down in first person (by the game or a mod), you are put back into first person once you are up",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Hands", "FirstPersonAfterKnockdown",
		+[](const Config& c) { return c.hands.firstPersonAfterKnockdown ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.hands.firstPersonAfterKnockdown = v != 0.0f; },
	},
	{
		"Comfort", "Game's animations from your head",
		"Sitting down, getting up, a fall, the mount, a feed: seen from your head; off, in third person from behind you",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "FirstPersonAnimations",
		+[](const Config& c) { return c.look.firstPersonAnimations ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.firstPersonAnimations = v != 0.0f; },
	},
	{
		"Comfort", "Animation vignette", "Darkens the edges of the view while the game plays one of those animations",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "AnimationVignette",
		+[](const Config& c) { return c.look.animationVignette ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.animationVignette = v != 0.0f; },
	},
	{
		"Comfort", "Animation vignette strength", "How dark the animation vignette gets, 0 to 1",
		ItemKind::Number, 0.0f, 1.0f, 0.1f, 1, false,
		"Look", "AnimationVignetteStrength",
		+[](const Config& c) { return c.look.animationVignetteStrength; },
		+[](Config& c, float v) { c.look.animationVignetteStrength = v; },
	},
	// ---- Looking -------------------------------------------------------------

	{
		"Looking", "Headset steers the view", "Off: the mouse and the stick turn and tilt the view, the head does nothing",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Head", "HeadLook",
		+[](const Config& c) { return c.tracker.headLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.headLook = v != 0.0f; },
	},
	{
		"Looking", "Headset turns the view only", "The mouse and the stick aim; the head looks around the crosshair",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "HeadTurnsViewOnly",
		+[](const Config& c) { return c.headTurnsViewOnly ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.headTurnsViewOnly = v != 0.0f; },
	},
	{
		"Looking", "Head steers the vertical look", "Up and down from the head, not the mouse; forced on with Aim at source, off without headset look",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "BlockVerticalLook",
		+[](const Config& c) { return c.look.blockVerticalLook ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.look.blockVerticalLook = v != 0.0f; },
	},
	// ---- Aiming --------------------------------------------------------------

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
	// ---- Dialogue ------------------------------------------------------------

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
	// ---- Screen & menus ------------------------------------------------------

	{
		"Screen & menus", "HUD stands in the room", "Off carries it on your head instead",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "HudAnchor",
		+[](const Config& c) { return c.tracker.hudAnchorWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.hudAnchorWorld = v != 0.0f; },
		"head", "world",
	},
	{
		"Screen & menus", "HUD distance", "Metres. How far away the flat picture hangs",
		ItemKind::Number, 0.4f, 8.0f, 0.1f, 1, false,
		"Render", "HudDistanceMetres",
		+[](const Config& c) { return c.tracker.hudDistanceMetres; },
		+[](Config& c, float v) { c.tracker.hudDistanceMetres = v; },
	},
	{
		"Screen & menus", "HUD width", "Metres. How wide it is at that distance",
		ItemKind::Number, 0.4f, 8.0f, 0.1f, 1, false,
		"Render", "HudWidthMetres",
		+[](const Config& c) { return c.tracker.hudWidthMetres; },
		+[](Config& c, float v) { c.tracker.hudWidthMetres = v; },
	},
	{
		"Screen & menus", "Menu scale", "How much of the picture a menu fills",
		ItemKind::Number, 0.3f, 2.0f, 0.05f, 2, false,
		"Render", "MenuScale",
		+[](const Config& c) { return c.tracker.menuScale; },
		+[](Config& c, float v) { c.tracker.menuScale = v; },
	},
	{
		"Screen & menus", "Menus in the world", "Menus hang in front of you instead of on the screen",
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
		"Screen & menus", "Live 3D pause world", "Redraw the paused Esc/Tab world from the HMD pose",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "LiveMenuBackground",
		+[](const Config& c) { return c.tracker.liveMenuBackground ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.liveMenuBackground = v != 0.0f; },
	},
	{
		"Screen & menus", "Unpaused menus", "The world keeps running behind inventory, map, magic, stats",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "UnpausedMenus",
		+[](const Config& c) { return c.tracker.unpausedMenus ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.unpausedMenus = v != 0.0f; },
	},
	{
		"Screen & menus", "Container over the chest", "A container's menu opens over the container, the world running",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Look", "ContainerInWorld",
		+[](const Config& c) { return c.containerPanel.inWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.containerPanel.inWorld = v != 0.0f; },
	},
	{
		"Screen & menus", "Container panel size", "The container panel's size, times the menus'",
		ItemKind::Number, 0.3f, 1.5f, 0.05f, 2, false,
		"Look", "ContainerPanelScale",
		+[](const Config& c) { return c.containerPanel.scale; },
		+[](Config& c, float v) { c.containerPanel.scale = v; },
	},
	{
		"Screen & menus", "Container panel height", "How far (m) the panel's middle sits over the container's middle",
		ItemKind::Number, -0.5f, 1.0f, 0.05f, 2, false,
		"Look", "ContainerPanelRaiseMetres",
		+[](const Config& c) { return c.containerPanel.raiseMetres; },
		+[](Config& c, float v) { c.containerPanel.raiseMetres = v; },
	},
	{
		"Screen & menus", "Flat picture depth (m)",
		"How far away the main menu, loading screens and the cinema screen are shown; 0 is infinity. Takes effect as "
		"the eye copies are remade",
		ItemKind::Number, 0.0f, 10.0f, 0.5f, 1, false,
		"Render", "FlatDepthMetres",
		+[](const Config& c) { return c.tracker.flatDepthMetres; },
		+[](Config& c, float v) { c.tracker.flatDepthMetres = v; },
	},
	{
		"Screen & menus", "Menu shade", "The vanilla brown wash behind a menu",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "MenuShade",
		+[](const Config& c) { return c.tracker.menuShade ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.menuShade = v != 0.0f; },
	},
	{
		"Screen & menus", "Black loading screens", "Black behind the loading art, not the frozen world and hands",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"Render", "LoadingBlack",
		+[](const Config& c) { return c.tracker.loadingBlack ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.tracker.loadingBlack = v != 0.0f; },
	},
	{
		"Screen & menus", "Shade strength", "How strong that wash is",
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
		"Screen & menus", "Stands in the room", "Off carries this panel on your head instead",
		ItemKind::Toggle, 0.0f, 1.0f, 1.0f, 0, false,
		"SettingsMenu", "Anchor",
		+[](const Config& c) { return c.settingsMenuInWorld ? 1.0f : 0.0f; },
		+[](Config& c, float v) { c.settingsMenuInWorld = v != 0.0f; },
		"head", "world",
	},
	{
		"Screen & menus", "Distance", "Metres in front of you",
		ItemKind::Number, 0.3f, 4.0f, 0.05f, 2, false,
		"SettingsMenu", "DistanceMetres",
		+[](const Config& c) { return c.settingsMenuDistanceMetres; },
		+[](Config& c, float v) { c.settingsMenuDistanceMetres = v; },
	},
	{
		"Screen & menus", "Width", "Metres wide at that distance",
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
		"Picture & performance", "Grass distance", "Where the grass ends, in game units (70 a metre). 0: the game's own. Most 8000",
		ItemKind::Number, 0.0f, 8000.0f, 500.0f, 0, false,
		"Render", "GrassDistance",
		+[](const Config& c) { return c.grassDistance; },
		+[](Config& c, float v) { c.grassDistance = v; },
	},
	{
		"Picture & performance", "Grass density (%)", "How much grass; 100 is the game's default, 200 about twice. 0: the game's own",
		ItemKind::Number, 0.0f, 400.0f, 25.0f, 0, true,
		"Render", "GrassDensity",
		+[](const Config& c) { return c.grassDensity; },
		+[](Config& c, float v) { c.grassDensity = v; },
	},
	{
		"Picture & performance", "Render width", "Pixels per eye. 0 asks the headset",
		ItemKind::Number, 0.0f, 8192.0f, 64.0f, 0, true,
		"Render", "GameResolutionWidth",
		+[](const Config& c) { return static_cast<float>(c.tracker.renderWidth); },
		+[](Config& c, float v) { c.tracker.renderWidth = static_cast<UInt32>(v); },
	},
	{
		"Picture & performance", "Render height", "Pixels per eye. 0 asks the headset",
		ItemKind::Number, 0.0f, 8192.0f, 64.0f, 0, true,
		"Render", "GameResolutionHeight",
		+[](const Config& c) { return static_cast<float>(c.tracker.renderHeight); },
		+[](Config& c, float v) { c.tracker.renderHeight = static_cast<UInt32>(v); },
	},
};

}  // namespace obvr::ui::settings_rows
