#pragma once

#include "core/Types.h"
#include "game/GameTypes.h"

namespace obvr::game {

// The charged spell in the hand (vr/SpellCharge.h SpellOrbFor): a copy of the spell's own projectile
// model - its first effect's MGEF model, e.g. MagicEffects\Fireball.NIF for Flare, the model the
// engine flies at the release - at the casting palm, at the size the charge has reached, playing the
// bolt's own sequence (game/OwnedNode.h PlayNodeSequence). The engine itself shows nothing in the
// hand between the cast animation's end and the release (spell-charge-look.txt, 2026-11-23).
//
// Hung on the casting hand's bone, at the palm's middle: the pins move that bone just before the draw
// and carry it along (CameraHook keeps the bone's bound in view, as the hands'). Not on "magicNode",
// where the engine hangs its own hand effect: it empties that node each frame no cast runs
// (0x005F42EF). Made the way the hips' weapons are (LoadModelCopy, SettleNewNode), kept while the
// model stays the same and hidden by the engine's cull bit between charges.

// What the charge wants this frame: shown, at what share of the model's own size, and whether that
// is the full size (for the log). A want not said again for two frames lapses (the controls not
// stepped), as the held release does.
void WantSpellOrb(bool shown, float scale, bool full);

// Once a frame: shows, sizes or hides the copy; `modelPath` is the active spell's model (nullptr or
// empty when it has none).
void StepSpellOrb(const char* modelPath);

// Whether `node` is the orb's copy (shown or hidden): what the hand's held-item read leaves out.
bool IsSpellOrbNode(const NiAVObject* node);

}  // namespace obvr::game
