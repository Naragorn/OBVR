#include "camera/hands/ThrowNoiseRun.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/RefFingerprint.h"
#include "game/ThrowNoiseLogic.h"
#include "game/WalkersLink.h"

namespace obvr::camera::hands {
namespace {

// The thrown thing's noise for Walkers (game/ThrowNoiseLogic.h): its first
// hard stop, else where it comes to rest - once, as 'WNOI'.
struct ThrowNoiseRun {
	game::ThrowNoise noise;
	game::RefFingerprint print;
};

ThrowNoiseRun& NoiseRun() {
	static ThrowNoiseRun s_run;
	return s_run;
}

}  // namespace

void StartThrowNoiseFor(UInt32 thrown, const Config& config) {
	if (thrown == 0 || !config.hands.throwNoise) {
		return;
	}
	game::StartThrowNoise(NoiseRun().noise, thrown);
	NoiseRun().print = game::TakeRefFingerprint(thrown);
}

void StepThrowNoiseFor(const Config& config, float dt, bool inWorld) {
	if (NoiseRun().noise.ref == 0) {
		return;
	}
	const UInt32 node = inWorld && game::RefStillThere(NoiseRun().print)
	                        ? *reinterpret_cast<const UInt32*>(NoiseRun().noise.ref + addr::kRefNiNodeOffset)
	                        : 0;
	if (!mem::LooksLikeObjectAddress(node)) {
		NoiseRun().noise = game::ThrowNoise{};
		return;
	}
	const UInt32 ref = NoiseRun().noise.ref;
	const float* const p = reinterpret_cast<const float*>(node + addr::kNodeWorldTranslateOffset);
	const game::NoiseStep step =
		game::StepThrowNoise(NoiseRun().noise, NiPoint3{p[0], p[1], p[2]}, dt, config.tracker.unitsPerMetre);
	if (!step.emit) {
		return;
	}
	const game::WalkersNoiseMessage msg = game::NoiseMessageFor(step.at, game::kNoiseThrow);
	const bool taken = game::SendWalkersNoise(msg.x, msg.y, msg.z, msg.kind);
	static UInt32 s_noiseLines = 16;
	if (s_noiseLines > 0) {
		--s_noiseLines;
		OBVR_LOG("Throw noise: %08X made its noise at %.1f %.1f %.1f - 'WNOI' to Walkers %s", ref,
		         static_cast<double>(msg.x), static_cast<double>(msg.y), static_cast<double>(msg.z),
		         taken ? "taken" : "not taken (Walkers absent)");
	}
}

}  // namespace obvr::camera::hands
