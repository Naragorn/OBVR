#pragma once

// The torch kept in the other hand with a two-hander drawn
// (game/TorchKeepLogic.h): both sites in 0x005E7230 jump to stubs that skip
// the engine's hide for the player while OBVR's flag is set.

namespace obvr::game {

// Checks the sites' bytes, builds the stubs and writes the jumps; logs
// what it did. Without a match nothing is written and the torch is put away
// as vanilla does.
void InstallTorchKeep();

// The flag the stubs read: set each frame while the torch is to stay
// ([Hands] TorchWithTwoHander with Full VR on).
void SetTorchKeep(bool keep);

// Whether the stubs are in.
bool TorchKeepInstalled();

}  // namespace obvr::game
