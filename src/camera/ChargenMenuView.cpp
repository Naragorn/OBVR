#include "camera/ChargenMenuView.h"

#include "camera/ChargenView.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/EngineMovie.h"
#include "game/MenuMode.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/PlayerTeleport.h"

namespace obvr::camera {

namespace {

// The opening's head, held while the menu stays up.
ChargenAlign& HeldAlign() {
	static ChargenAlign s_align{};
	return s_align;
}

// The opening's head against the recentre, then where the face lies from the
// view now and then while the menu stays up - so a character off to the side
// can be read in the log (Nadi, 2026-11-13: "zu seitlich, musste nach rechts
// gucken").
void LogChargenAlign(bool wasHeld, float faceYaw) {
	static UInt32 s_openings = 8;
	static UInt32 s_frames = 0;
	static UInt32 s_lines = 0;
	if (!wasHeld) {
		s_frames = 0;
		s_lines = 6;
		if (s_openings > 0) {
			--s_openings;
			OBVR_LOG("Chargen view: %s - the head turned %.1f degrees from the recentre (left positive), "
			         "stood %.1f %.1f units from its place; the face %.1f degrees from the view (left positive; "
			         "the engine's own aim -20.1)",
			         HeldAlign().held ? "laid before the head as it stands" : "not laid before the head (ChargenFacesHead off or no head read)",
			         static_cast<double>(math::Atan2(HeldAlign().head.sine, HeldAlign().head.cosine) *
			                             math::kRadiansToDegrees),
			         static_cast<double>(HeldAlign().offset.x), static_cast<double>(HeldAlign().offset.y),
			         static_cast<double>(faceYaw));
		}
		return;
	}
	++s_frames;
	if (s_lines > 0 && s_frames % 450 == 0) {
		--s_lines;
		OBVR_LOG("Chargen view: the face %.1f degrees from the view (left positive), %u frames into the menu",
		         static_cast<double>(faceYaw), s_frames);
	}
}

}  // namespace

void ChargenMenuBaseFor(const Config& config, const NiMatrix33& menuBaseRot, const NiPoint3& menuBasePos,
                        bool menuBaseThirdPerson, const vr::HeadTracker& tracker,
                        NiMatrix33 (*headOnBase)(const NiMatrix33&), NiMatrix33& baseRotation, NiPoint3& basePos,
                        float& verticalOffset, NiPoint3& headOffset) {
	// The last world frame's base is the engine's chase camera behind the
	// body here, the engine having switched to third person for the menu.
	// The face is read as the engine reads it (the FaceGen node's bound
	// centre), the facing is the last world frame's heading, the distance
	// the setting.
	static bool s_chargenSaid = false;
	static UInt32 s_chargenLines = 12;
	ChargenViewInput in;
	in.wanted = config.look.chargenView;
	in.raceMenuUp = game::IsMenuMode() && game::TopVisibleMenu() == game::kMenuIdRaceSex;
	if (in.raceMenuUp) {
		in.faceKnown = game::PlayerFaceCentre(in.face);
		in.facingKnown = HeadingOf(menuBaseRot, in.facing);
		in.distanceUnits = config.look.chargenDistanceMetres * config.tracker.unitsPerMetre;
	}
	const ChargenViewVerdict chargen = ChargenView(in);
	// Before the head as it stands at the opening: the head's turn and
	// place are measured from the last recentre (ChargenAlign).
	Heading headHeading{};
	const bool headKnown = config.look.chargenFacesHead && tracker.IsHeadsetConnected() &&
	                       HeadingOf(tracker.GetCameraRotation(), headHeading);
	const bool wasHeld = HeldAlign().held;
	HeldAlign() = StepChargenAlign(HeldAlign(), chargen.active, headKnown, game::EngineMoviePlaying(), headHeading,
	                               tracker.GetCameraOffset());
	if (chargen.active) {
		baseRotation = ChargenAlignedBase(chargen.rot, HeldAlign());
		basePos = chargen.pos;
		verticalOffset = 0.0f;
		headOffset = ChargenAlignedOffset(tracker.GetCameraOffset(), HeldAlign());
		const NiPoint3 eyes = basePos + baseRotation * headOffset;
		LogChargenAlign(wasHeld, ChargenFaceYawDegrees(eyes, headOnBase(baseRotation), in.face));
	}
	if (in.raceMenuUp && !s_chargenSaid && s_chargenLines > 0) {
		s_chargenSaid = true;
		--s_chargenLines;
		if (chargen.active) {
			OBVR_LOG("Chargen view: the race menu is up - the eyes %.1f units (%.2f m) in front of the face at "
			         "%.1f %.1f %.1f, facing it, aimed %.1f units beside it as the game's own camera (30 and 11); "
			         "the last world frame's base %.1f %.1f %.1f, %s",
			         static_cast<double>(in.distanceUnits), static_cast<double>(config.look.chargenDistanceMetres),
			         static_cast<double>(in.face.x), static_cast<double>(in.face.y), static_cast<double>(in.face.z),
			         static_cast<double>(in.distanceUnits * kEngineChargenAimSideUnits / kEngineChargenDistanceUnits),
			         static_cast<double>(menuBasePos.x), static_cast<double>(menuBasePos.y),
			         static_cast<double>(menuBasePos.z), menuBaseThirdPerson ? "third person" : "first person");
		} else {
			OBVR_LOG("Chargen view: the race menu is up but the view stays on the last world frame's base %.1f %.1f "
			         "%.1f (%s) - the setting %d, the face %s, the facing %s, the distance %.1f units",
			         static_cast<double>(menuBasePos.x), static_cast<double>(menuBasePos.y),
			         static_cast<double>(menuBasePos.z), menuBaseThirdPerson ? "third person" : "first person",
			         config.look.chargenView ? 1 : 0, in.faceKnown ? "read" : "unread",
			         in.facingKnown ? "read" : "unread", static_cast<double>(in.distanceUnits));
		}
	} else if (!in.raceMenuUp) {
		s_chargenSaid = false;
	}
}

}  // namespace obvr::camera
