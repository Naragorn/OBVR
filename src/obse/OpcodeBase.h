#pragma once

// The opcode base OBVR claims from xOBSE at load.
//
// OBVR registers no script commands, so it never needed one - until the
// co-save: xOBSE keys a plugin's chunk in the .obse file by the plugin's
// opcode base, and HandleSaveGame skips the save callback of a plugin whose
// base was never set (xOBSE obse/Serialization.cpp: `if(!s_pluginHeader.
// opcodeBase) { _ERROR("HandleSaveGame: plugin with default opcode base
// registered for serialization"); continue; }`). The load callback still
// runs, in the pass for plugins without data. So the quick-key pages
// (game/QuickKeyPages.cpp, record 'QKPG') were read as "none in the co-save"
// on every load and never written - found by the OblivionWalkers session on
// 2026-10-24 from its own obse.log.
//
// The base: xOBSE's SetOpcodeBase asserts 0x2000 <= base < 0x8000 and warns
// on 0x2000, the default of its example plugin (PluginManager.cpp). The
// ranges plugins have registered end at 0x2A24 (github.com/llde/xOBSE/wiki/
// Registered-Opcodes-for-OBSE-Plugins, revision of 2026-02-18);
// OblivionWalkers took 0x2B00 to 0x2B0F. OBVR takes 0x2C00 to 0x2C0F,
// unregistered on the wiki as yet. Only loaded plugins can collide, and only
// on the same base.
//
// Pure apart from the one call into xOBSE; covered by opcode_base_test.

#include "core/Types.h"
#include "obse/PluginInterface.h"

namespace obvr::obse {

inline constexpr UInt32 kOpcodeBase = 0x2C00;

enum class OpcodeBaseVerdict : UInt8 {
	Own,         // a base xOBSE takes as the plugin's: the co-save chunk has its key
	Default,     // 0x2000: xOBSE's default, the save callback would be skipped
	OutOfRange,  // outside 0x2000 to 0x7FFF: xOBSE asserts
	NoCall,      // no interface, or one without SetOpcodeBase (OBSE before 0015)
};

inline OpcodeBaseVerdict JudgeOpcodeBase(UInt32 base) {
	if (base < 0x2000 || base >= 0x8000) {
		return OpcodeBaseVerdict::OutOfRange;
	}
	return base == 0x2000 ? OpcodeBaseVerdict::Default : OpcodeBaseVerdict::Own;
}

// Claims `base` with xOBSE when it is one xOBSE takes; anything else is
// refused without a call.
inline OpcodeBaseVerdict ClaimOpcodeBase(const Interface* obse, UInt32 base) {
	if (obse == nullptr || obse->SetOpcodeBase == nullptr) {
		return OpcodeBaseVerdict::NoCall;
	}
	const OpcodeBaseVerdict verdict = JudgeOpcodeBase(base);
	if (verdict == OpcodeBaseVerdict::Own) {
		obse->SetOpcodeBase(base);
	}
	return verdict;
}

}  // namespace obvr::obse
