#pragma once

// Whether the engine is playing a film (a Bink movie: the new game's intro,
// the main menu's map loop, the credits, the Shivering Isles' dreams), read
// from the movie player's current record (game/GameAddresses.h,
// kCurrentMovie). The headset's delivery hands the cinema screen to a film
// that is the picture (camera/FrameLogic.h, MovieOwnsScreen).

#include "core/Types.h"

namespace obvr::game {

bool EngineMoviePlaying();

// The record's state at +0x20 as the engine's readers take it (read 2026-11-15 in the pseudocode of 0x0057F170,
// 0x005ADBC0, 0x005B5870 and 0x006AE860): 0 no film, 2 a film behind a menu - the main menu's map loop, which the
// interface pass 0x0057F170 draws at its start only while the state is 2 - anything else a film that is the
// picture (the sounds held, the menu's music stopped).
constexpr UInt32 kMovieStateNone = 0;
constexpr UInt32 kMovieStateBehindMenu = 2;

// The state to write so that the main menu's film is not drawn while OBVR's guide stands in its dome, and is
// drawn again afterwards (Nadi, 2026-11-15: the guide's window had the main menu's map round it on the menu's
// quad): held, a film behind a menu reads "no film" - every reader above takes 0 as it takes 2, but the pass
// that draws it; let go, a record OBVR holds goes back to 2. Any other state is left as it is. Pure.
constexpr UInt32 MenuFilmState(bool hold, UInt32 state, bool heldByUs) {
	if (hold) {
		return state == kMovieStateBehindMenu ? kMovieStateNone : state;
	}
	return heldByUs && state == kMovieStateNone ? kMovieStateBehindMenu : state;
}

// Holds the main menu's film off the picture, or lets it go (MenuFilmState); called every frame the guide's
// stage is up, and once when it ends. True while a record is held.
bool HoldMenuFilm(bool hold);

}  // namespace obvr::game
