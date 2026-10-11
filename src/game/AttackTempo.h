#pragma once

#include "game/AttackTempoLogic.h"

namespace obvr::game {

// Enemy attacks slower and rarer, the engine side (AttackTempoLogic.h has
// the decisions and the reading behind them).
//
// Install patches, each checked against the bytes it expects and left alone
// when they differ: the key-firing call in the anim update (0x00477AB1) goes
// through the wrapper that keeps a followed sequence's key window
// continuous; the combat style's two attack-chance multiplier getters
// (0x004AA0B0, 0x004AA130) and Actor::GetCombatStyle (0x005E0F50) are
// detoured at their entries; the two hold-timer draws (0x00619176,
// 0x00620BFB) are re-pointed at the wrapper that scales them.
void InstallAttackTempo();

// Once a frame: the settings the hooks apply from now on, and the attack
// sequences of those fighting the player followed - their time run at the
// speed asked. Off, or nothing to change: nothing followed, the hooks hand
// the game's own values through.
void StepAttackTempo(const AttackTempoSettings& settings, bool logState);

}  // namespace obvr::game
