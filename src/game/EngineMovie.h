#pragma once

// Whether the engine is playing a film (a Bink movie: the new game's intro,
// the main menu's map loop, the credits, the Shivering Isles' dreams), read
// from the movie player's current record (game/GameAddresses.h,
// kCurrentMovie). The headset's delivery hands the cinema screen to a film
// that is the picture (camera/FrameLogic.h, MovieOwnsScreen).

namespace obvr::game {

bool EngineMoviePlaying();

}  // namespace obvr::game
