#pragma once

// The start guide comes back once with every new beta, and with a release
// whose guide got new rows (Nadi, 2026-11-25: "bei jeder neuen beta das
// onboarding für leute wieder anschalten einmalig. wenn wir dem onboarding
// neue toggles hinzufügen bei releases auch"). A player who keeps their
// OBVR.ini across an update never saw the guide's new rows nor a changed
// default otherwise: that day [Hands] TakeOnlyByHand went to 0 by default,
// and an INI kept from beta 9 still says 1 - only the guide's comfort page,
// which offers the row, puts the choice in front of that player.
//
// kGuideRevision is raised by hand: with every beta packed (the packer
// refuses a beta at a revision it has packed another beta with,
// tools/package-release.py), and with every change to the rows the guide's
// pages offer (native_settings_test pins them beside the revision, and
// fails when they change and it does not). The player's OBVR.ini keeps the
// revision last offered there ([Onboarding] GuideRevision, written by OBVR,
// game/GuideReturn.cpp); a lower one, or none, switches the guide on
// ([Onboarding] StartGuide=1) and writes this one. From there the guide runs
// as on a first start: come through to its comfort page it switches itself
// off, ended by Esc it comes again at the next start (2026-11-06).
//
// Revision 1 is the guide of beta 9 and before (no GuideRevision written).
//
// Pure, covered by native_settings_test.

#include "core/Types.h"

namespace obvr::ui {

inline constexpr UInt32 kGuideRevision = 2;

// Whether the guide comes back for a player whose OBVR.ini names `seen`
// (0: none written yet) - and so whether `current` is to be written there.
inline bool GuideComesBack(UInt32 seen, UInt32 current) { return seen < current; }

}  // namespace obvr::ui
