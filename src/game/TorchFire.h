#pragma once

// A torch let go of in the world, still burning (game/TorchRelease.h; Nadi,
// 2026-10-26). Vanilla drops "a lightless log" (the Nexus drop-lit-torch
// mods: TorchLight for the remaster, DLTIC 3429 and ROM DLT 34644 for the
// original - they swap in a second light record or move a light of their
// own to it). OBVR lights the dropped reference itself with the engine's own
// calls, read in Oblivion.exe 2026-10-26 (objdump):
//
//   0x0046AB60  TESObjectREFR, thiscall(bool): sets (1) or clears (0) the form
//               flag 0x80 - a light reference's fire out. A new reference of
//               a light starts with it as the light's "off by default" bit
//               (0x0044A968); the 3D's set-up reads it: set, the flames are
//               taken off (0x004E41DA), clear, they are put on.
//   0x004E26F0  cdecl(NiNode*): the flames put on - each "FlameNode<n>" gets
//               the flame model of index n, "FlameCap" shown; recursive.
//   0x004DE1C0  cdecl(NiNode*): the flames taken off, "FlameCap" hidden.
//   0x004B1600  TESObjectLIGH, thiscall(ref, node): the light made under the
//               node's "AttachLight" and handed to the scene - the same call
//               the 3D's set-up makes for a light reference (0x004DF53E) and
//               the body makes for a torch in the hand (0x00478270).
//
// Torch02.nif has an "AttachLight" node, an empty "FlameNode1" and a
// "FlameCap" (read with pyffi 2026-10-26). Which of the engine's steps a
// dropped torch misses is measured by the log lines this file writes
// ("Torch: the dropped ... before"), not read.

#include "core/Types.h"
#include "game/FlameStateLogic.h"
#include "game/NiMath.h"

namespace obvr::game {

// What a torch reference's 3D carries.
struct TorchFireReading {
	bool haveNode = false;
	UInt32 formFlags = 0;     // the reference's; 0x80 is fire out
	UInt32 lightFlags = 0;    // the base light's (+0x7C): 0x2 carried, 0x20 off by default
	UInt32 flameNodes = 0;    // nodes named "FlameNode<n>"
	UInt32 flames = 0;        // children under them
	bool capShown = false;    // "FlameCap" there and not hidden
	bool capFound = false;
	UInt32 lightsUnder = 0;   // children of "AttachLight"
	char lightClass[40] = {}; // the first one's class
	NiPoint3 position{0.0f, 0.0f, 0.0f};  // the node's, world units
	bool hidden = false;      // the node's app-culled flag
};

bool ReadTorchFire(UInt32 ref, TorchFireReading& out);

// Puts the fire on a dropped torch: the fire-out flag cleared, the flames
// put on (with `putFlamesOn`, when there are none), and the light made when
// none hangs under "AttachLight" yet. False, with the reason in `why`, when
// the reference is no light with a 3D, or the engine's functions are not the
// ones read.
//
// The flames only on a 3D the engine has just made - the dropped torch in its
// first frame: the engine sets the new shapes' render state up when it hangs
// that 3D into the cell. Put on a 3D already in the scene they draw without
// it, and the renderer faults (NiDX9RenderState on the flame's NiTriShape,
// torch-drop-relight.txt 2026-10-26 09:02). A 3D the engine built anew has
// its flames already - the cleared flag is read by its set-up (0x004E41DA) -
// and lacks only the light.
bool IgniteDroppedTorch(UInt32 ref, bool putFlamesOn, char* why, UInt32 whySize);

// The instrument for the crash of 2026-10-26 (game/FlameStateLogic.h): a
// torch model's shapes and whether each has its render state - the dropped
// torch's by its reference, the one in the hand under the first-person tree
// (the first node named like a torch). False when there is no such model.
bool ReadDroppedTorchShapeStates(UInt32 ref, FlameShapeStates& out);
bool ReadFirstPersonTorchShapeStates(FlameShapeStates& out);

// The torches lying in the player's cell walked (game/TorchGroundLogic.h):
// those OBVR lit whose 3D came back without its light get it again.
struct TorchCellScan {
	UInt32 cell = 0;        // the cell's form id
	UInt32 carried = 0;     // carried lights lying in it
	UInt32 ours = 0;        // of them, with the fire on: lit by OBVR
	UInt32 waiting = 0;     // of those, their 3D or flames not there yet
	UInt32 relit = 0;       // given their light again now
	UInt32 firstRelit = 0;  // the first of those, its form id
};

// False when the player has no cell to walk.
bool ScanTorchesInPlayerCell(TorchCellScan& out);


}  // namespace obvr::game
