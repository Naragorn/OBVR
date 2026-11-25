#pragma once

namespace obvr::game {

// Once at the plugin's load, after OBVR.ini is read: the start guide switched
// on again when this OBVR's guide revision is newer than the one the player's
// OBVR.ini names (ui/GuideRevision.h), and the revision written there. Not in
// a harness run - its INI keeps the guide off, and the player's file is left
// alone.
void ReturnGuideForNewRevision();

}  // namespace obvr::game
