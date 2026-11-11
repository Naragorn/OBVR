#include "game/EngineMovie.h"

#include "core/AddressSpace.h"

namespace obvr::game {
namespace {
// The engine's current film (read 2026-11-11 with REA, Oblivion.exe): the
// pointer at 0x00B33428 is the playing movie's record, 0x28 bytes, or null.
// The player 0x00410C40 (name, loop) pauses any current one into 0x00B3342C,
// allocates the record and opens the Bink file through 0x004105F0, which sets
// +0x20 to 2 and +0x00 to the HBINK (BinkOpen); on a failed open it sets
// +0x20 back to 0. The stop 0x00410B00 BinkCloses +0x00, zeroes the record,
// frees it and nulls 0x00B33428 (resuming a paused one into it). The main
// menu's constructor 0x005B5F20 reads the same +0x20 to decide whether to
// start "Map loop.bik". A film is playing while the pointer, its HBINK and
// its state are all non-zero.
constexpr UInt32 kCurrentMovie = 0x00B33428;
constexpr UInt32 kMovieBinkOffset = 0x00;
constexpr UInt32 kMovieStateOffset = 0x20;
}  // namespace

bool EngineMoviePlaying() {
	const UInt32 record = *reinterpret_cast<const UInt32*>(kCurrentMovie);
	if (!mem::LooksLikeObjectAddress(record)) {
		return false;
	}
	const UInt32 bink = *reinterpret_cast<const UInt32*>(record + kMovieBinkOffset);
	const UInt32 state = *reinterpret_cast<const UInt32*>(record + kMovieStateOffset);
	return bink != 0 && state != 0;
}

}  // namespace obvr::game
