#pragma once

#include "core/Types.h"

namespace obvr::game {

// The tutorial cards in Full VR's words (ui/TutorialCards.h; docs/
// onboarding-spec.md 3.4, step 2; the tester, 2026-10-16).
//
// A quest update's card (the Generic menu from quest_added.xml, "Quest
// updated") takes the stage entry's text from addr::kQuestStageEntryText
// (0x0052AF40): thiscall on the stage item with the quest as its argument,
// it reads the entry's CNAM out of the quest's record in the file on demand
// into the string at 0xB36300 and answers that text, or nullptr. Its entry
// is detoured: with the mode on, an entry that is one of the Tutorial
// quest's cards OBVR has words for is answered with OBVR's text instead, a
// constant of the DLL the window copies into its tile. Everything else, and
// everything with the mode off, is the engine's own answer. The journal's
// quest pages, if they read entries the same way, show the same words.

// Detours the reader's entry; logs its outcome. False when the bytes are not
// the ones read (another mod there) - then the cards keep their own words.
bool InstallTutorialCardsHook();
bool TutorialCardsHookInstalled();

// Whether OBVR's cards are wanted this frame (the mode on).
void SetTutorialCardsWanted(bool wanted);

}  // namespace obvr::game
