#include "camera/hands/ScriptPicture.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/ActorRoster.h"
#include "game/ActorRosterLogic.h"
#include "game/BodyView.h"
#include "game/CrosshairTarget.h"
#include "game/GameAddresses.h"
#include "game/HandBodies.h"
#include "game/InventoryGrab.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
#include "game/TakeItem.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {

void LogHandScriptPicture(const HandStep& step, const HandShared& shared, const vr::HandModeFrame& frame,
                          const game::NearItem& nearItem) {
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& handResult = shared.hand;
	const NiTransform& cyclopeanCamera = shared.cyclopeanCamera;
	const bool cyclopeanCameraValid = shared.cyclopeanCameraValid;
	// What a hand script's picture is of ([Debug] HandScript): the state on
	// the frame of each mark.
	if (test::HandScriptMarkedThisFrame()) {
		// Where the player stands and faces (the engine's yaw, forward = sin,
		// cos): what a walk between two marks is measured against.
		NiPoint3 feet{0.0f, 0.0f, 0.0f};
		game::PlayerRotation turned{};
		if (game::ReadPlayerFeet(feet) && game::ReadPlayerRotation(turned)) {
			OBVR_LOG("HandScript: player at %.1f %.1f %.1f, heading %.1f degrees",
			         static_cast<double>(feet.x), static_cast<double>(feet.y), static_cast<double>(feet.z),
			         static_cast<double>(turned.yaw * math::kRadiansToDegrees));
		}
		game::LogHandBodies();
		OBVR_LOG("HandScript: state - world %d, menu %d, third person %d, head %d at %.2f %.2f "
		         "%.2f, right %d at %.2f %.2f %.2f stick %.2f %.2f, left %d, camera %d, hand "
		         "pinned %d, teleport allowed %d aiming %d commit %d, weapon %d (slot %d), active menu %s, laser hit %d at %.0f,%.0f, cursor %d at %.0f,%.0f, top menu %s",
		         frame.inWorld ? 1 : 0, menuIsUp ? 1 : 0, frame.firstPerson ? 0 : 1,
		         frame.headValid ? 1 : 0, static_cast<double>(frame.headPosition.x),
		         static_cast<double>(frame.headPosition.y),
		         static_cast<double>(frame.headPosition.z), frame.right.valid ? 1 : 0,
		         static_cast<double>(frame.right.position.x),
		         static_cast<double>(frame.right.position.y),
		         static_cast<double>(frame.right.position.z),
		         static_cast<double>(frame.right.thumbX), static_cast<double>(frame.right.thumbY),
		         frame.left.valid ? 1 : 0, cyclopeanCameraValid ? 1 : 0,
		         handResult.rightHandValid ? 1 : 0, frame.teleportAllowed ? 1 : 0,
		         handResult.teleportAiming ? 1 : 0, handResult.teleportCommit ? 1 : 0,
		         static_cast<int>(frame.weaponSeen), static_cast<int>(frame.equipped),
		         menuIsUp ? game::MenuIdName(game::ActiveMenuId()) : "none", handResult.laserHit ? 1 : 0,
		         static_cast<double>(handResult.laserPixelX), static_cast<double>(handResult.laserPixelY),
		         frame.cursorValid ? 1 : 0, static_cast<double>(frame.cursorX),
		         static_cast<double>(frame.cursorY), game::MenuIdName(game::TopVisibleMenu()));
		if (frame.flat.valid) {
			OBVR_LOG("HandScript: the flat picture anchored at %.2f %.2f %.2f facing %.2f %.2f %.2f, %.2f by %.2f "
			         "(tangents), the plane %.2f m",
			         static_cast<double>(frame.flat.centre.x), static_cast<double>(frame.flat.centre.y),
			         static_cast<double>(frame.flat.centre.z), static_cast<double>(frame.flat.forward.x),
			         static_cast<double>(frame.flat.forward.y), static_cast<double>(frame.flat.forward.z),
			         static_cast<double>(frame.flat.tanHalfWidth), static_cast<double>(frame.flat.tanHalfHeight),
			         static_cast<double>(frame.flatDepthMetres));
		}
		if (menuIsUp) {
			// What the beam is on, by the engine's own active tile: a script
			// aims at a menu's part by this.
			char underCursor[48];
			const bool bar = game::CursorOverScrollBar(underCursor, sizeof(underCursor));
			OBVR_LOG("HandScript: the tile under the cursor \"%s\"%s", underCursor, bar ? " (a scroll bar)" : "");
			UInt32 menuVtable = 0;
			UInt32 menuRawId = 0;
			if (game::ActiveMenuRaw(menuVtable, menuRawId)) {
				OBVR_LOG("HandScript: the active menu's vtable %08X, id %X", menuVtable, menuRawId);
			}
			for (UInt32 up = 0; up < 3; ++up) {
				UInt32 traitIds[48];
				float traitValues[48];
				const UInt32 traits = game::ActiveTileTraits(traitIds, traitValues, 48, up);
				char traitLine[48 * 20] = {};
				UInt32 at = 0;
				for (UInt32 i = 0; i < traits && at + 24 < sizeof(traitLine); ++i) {
					at += static_cast<UInt32>(std::snprintf(traitLine + at, sizeof(traitLine) - at, " %X=%.1f",
					                                        traitIds[i], static_cast<double>(traitValues[i])));
				}
				OBVR_LOG("HandScript: its traits (%u up):%s", up, traitLine);
			}
			// The inventory's item under the pointer (game/InventoryGrab.h): the
			// first step of taking it into the hand.
			game::InventoryItemUnder item;
			if (game::InventoryItemUnderCursor(item)) {
				OBVR_LOG("Inventory: under the pointer index %u - \"%s\" (form %08X, type %u)", item.index, item.name,
				         item.formId, static_cast<unsigned>(item.formType));
			} else if (game::ActiveMenuId() == game::kMenuIdInventory) {
				OBVR_LOG("Inventory: under the pointer no item");
			}
			float dragX = 0.0f;
			float dragY = 0.0f;
			float panX = 0.0f;
			float panY = 0.0f;
			if (game::MapMenuPan(dragX, dragY, panX, panY)) {
				OBVR_LOG("HandScript: the map's drag point %.0f,%.0f, its pan %.1f,%.1f", static_cast<double>(dragX),
				         static_cast<double>(dragY), static_cast<double>(panX), static_cast<double>(panY));
			}
		}
		// What the hands could take and what they hold: the grab and the stow
		// scenarios read whether an item was found at all.
		const NiPoint3 camera =
			cyclopeanCameraValid ? cyclopeanCamera.pos : NiPoint3{0.0f, 0.0f, 0.0f};
		game::SearchHand eyes;
		eyes.valid = cyclopeanCameraValid;
		eyes.position = camera;
		const game::NearItem anyItem =
			game::FindNearestItem(eyes, game::SearchHand{}, 2000.0f, 2000.0f, 0);
		// For the mark's console-at lines run from inside ({near}).
		test::NoteHandScriptNearestForm(mem::LooksLikeObjectAddress(anyItem.ref)
		                                    ? *reinterpret_cast<const UInt32*>(anyItem.ref + addr::kFormIdOffset)
		                                    : 0u);
		// Who stands in the world, by base form, alive and dead: what a
		// scenario that spawns, kills or raises people reads; the nearest
		// is {actor} for its console-at lines (game/ActorRosterLogic.h).
		{
			game::RosterActor actors[256];
			const UInt32 loaded = game::CollectLoadedActors(actors, 256);
			const int nearestActor = game::NearestRosterActor(actors, loaded);
			test::NoteHandScriptNearestActor(nearestActor >= 0 ? actors[nearestActor].formId : 0u);
			char roster[640];
			game::FormatRosterLine(actors, loaded, roster, sizeof(roster));
			OBVR_LOG("HandScript: actors - %s", roster);
			// Debug.WornShapes: what the nearest ones wear, as read
			// (game/BodyView.h, mesh-fitted-capsules-spec todo 2).
			if (GetConfig().body.wornShapes) {
				UInt32 nearest[6];
				const UInt32 n = game::RosterActorsNearestFirst(actors, loaded, 2000.0f, 6, nearest);
				for (UInt32 i = 0; i < n; ++i) {
					game::LogWornShapes(actors[nearest[i]].ref, actors[nearest[i]].formId, test::HandScriptMarkName());
				}
			}
		}
		const NiPoint3 hand = cyclopeanCamera.pos +
		                      cyclopeanCamera.rot * handResult.rightHandOffsetUnits;
		OBVR_LOG("HandScript: items - near a hand %d (ref %08X type %02X, %.0f units from the %s "
		         "hand), nearest to the eyes %08X form %08X type %02X at %.0f %.0f %.0f (%.0f "
		         "units); "
		         "eyes %.0f %.0f %.0f, right hand %.0f %.0f %.0f; held %08X, the crosshair on %08X; blocking %d, block "
		         "key %d, attack key %d, player action %d",
		         nearItem.valid ? 1 : 0, nearItem.ref,
		         game::RefStillThere(nearItem.print) ? game::RefBaseFormType(nearItem.ref) : 0u,
		         static_cast<double>(nearItem.distance), nearItem.left ? "left" : "right",
		         anyItem.ref,
		         mem::LooksLikeObjectAddress(anyItem.ref)
		             ? *reinterpret_cast<const UInt32*>(anyItem.ref + addr::kFormIdOffset)
		             : 0u,
		         game::RefBaseFormType(anyItem.ref),
		         static_cast<double>(anyItem.centre.x), static_cast<double>(anyItem.centre.y),
		         static_cast<double>(anyItem.centre.z), static_cast<double>(anyItem.distance),
		         static_cast<double>(camera.x), static_cast<double>(camera.y),
		         static_cast<double>(camera.z), static_cast<double>(hand.x),
		         static_cast<double>(hand.y), static_cast<double>(hand.z), game::GrabbedRef(),
		         game::ReadCrosshairTarget().haveRef ? game::ReadCrosshairTarget().refAddress : 0u,
		         handResult.blocking ? 1 : 0, handResult.controls.block ? 1 : 0,
		         handResult.controls.attack ? 1 : 0, static_cast<int>(game::ReadPlayerAction()));
	}

}

}  // namespace obvr::camera::hands
