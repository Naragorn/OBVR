#pragma once

// The weapon skill a strike by motion trains, and the exception list other
// plugins set (game/SkillCreditLogic.h for the why).

#include "core/Types.h"
#include "obse/PluginInterface.h"

namespace obvr::game {

// The message another plugin sends OBVR to replace the exception list:
// receiver "OBVR", type kSkillExceptionsMessage (the bytes 'S' 'K' 'E' 'X'
// in memory, as OBVR's co-save records spell theirs), data a UInt32 array of
// base form IDs as the running game has them, dataLen its size in bytes.
// Each message replaces the list; an empty one clears it.
inline constexpr UInt32 kSkillExceptionsMessage = 0x58454B53;  // 'SKEX'

// Listens to xOBSE: at its PostLoad, to every plugin, for the message above.
void InstallSkillMessages(const obse::Interface* obse);

// [Hands] StrikesTrainSkills, set every frame from the config.
void SetStrikesTrainSkills(bool enabled);

// After AttackHandling has struck `target` for the player: the counted hits
// credited to the attack's skill as the engine's attack update would, or
// cleared without a use for an excepted target. With StrikesTrainSkills off
// the count is left to the game, as before.
void CreditStrikeSkill(UInt8* player, void* target);

}  // namespace obvr::game
