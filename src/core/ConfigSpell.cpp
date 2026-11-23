#include "core/ConfigSpell.h"

#include "core/Config.h"
#include "core/ConfigRead.h"

namespace obvr {

void ReadSpellHandStep(Config& config, const char* path) {
	using config_read::ReadBool;
	using config_read::ReadFloat;
	vr::SpellAimSettings& a = config.hands.spellAim;
	a.enabled = ReadBool("Hands", "SpellFromCastingHand", a.enabled, path);
	a.raiseBelowEyesMetres = ReadFloat("Hands", "SpellRaiseBelowEyesMetres", a.raiseBelowEyesMetres, path);
	a.raiseAheadMetres = ReadFloat("Hands", "SpellRaiseAheadMetres", a.raiseAheadMetres, path);
	vr::SpellChargeSettings& c = config.hands.spellAim.charge;
	c.enabled = ReadBool("Hands", "SpellCharge", c.enabled, path);
	c.tapSeconds = ReadFloat("Hands", "SpellChargeTapSeconds", c.tapSeconds, path);
	c.stage2Seconds = ReadFloat("Hands", "SpellChargeStage2Seconds", c.stage2Seconds, path);
	c.stage3Seconds = ReadFloat("Hands", "SpellChargeStage3Seconds", c.stage3Seconds, path);
	// A tap of no time charges every press; the stages in their order after it.
	c.tapSeconds = c.tapSeconds < 0.05f ? 0.05f : c.tapSeconds;
	c.stage2Seconds = c.stage2Seconds < c.tapSeconds ? c.tapSeconds : c.stage2Seconds;
	c.stage3Seconds = c.stage3Seconds < c.stage2Seconds ? c.stage2Seconds : c.stage3Seconds;
}

}  // namespace obvr
