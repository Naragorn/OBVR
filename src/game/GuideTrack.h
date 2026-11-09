#pragma once

// The start guide's soft track and the game's title music (ui/GuideStage.h
// decides, this carries it out).
//
// The soft track plays in OBVR's own DirectShow graph - the same way the
// game plays its music (0x006AB160: CoCreateInstance of the FilterGraph,
// IGraphBuilder::RenderFile, IBasicAudio for the volume; the GUIDs read from
// the game's own image at 0x00A78F64..0x00A78FC3), kept apart from the game's
// music player so that player's own switching never sees it. Its volume and
// its position are OBVR's: IBasicAudio::put_Volume, IMediaPosition.
//
// The title music stays the game's: its player (*(0x00B33398)+0x24, read at
// 0x005B5B36) keeps playing it, OBVR only holds its IBasicAudio (+0x74) at
// silence while the guide is up and, when it ends, asks the game's own volume
// setter (0x006AA1A0) to put back the volume the game wants (with the title
// flag 0x00B3C0EC set it takes fMainMenuMusicVolume).

#include "core/Types.h"

namespace obvr::game {

// Opens the file (a path relative to the game's folder, ASCII) and plays it
// silent. False with a reason when it cannot.
bool OpenGuideTrack(const char* path, const char** why);
void CloseGuideTrack();
bool GuideTrackOpen();
// 0..1 of full.
void SetGuideTrackVolume(float volume);
// Seconds; -1 when not known.
float GuideTrackPosition();
float GuideTrackDuration();
void SeekGuideTrack(float seconds);

// The title music: held silent (call every frame while it should be; cheap
// when it already is) and given back. True when the game's player was there.
bool HoldTitleMusicSilent();
bool GiveTitleMusicBack();

// The main menu's own picture (its TileMenu's visible trait, 0x0FA1, through
// Tile::UpdateFloat 0x0058CEB0); the guide's windows are menus of their own
// and stay. False when the main menu is not loaded.
bool SetMainMenuShown(bool shown);
bool MainMenuLoaded();

}  // namespace obvr::game
