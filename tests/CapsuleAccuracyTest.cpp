// How well the fitted capsules lie on real outfits (game/CapsuleAccuracy.h):
// the bones and vertices the game's fit was given for Jauffre in his robe,
// in a mod's Dwarven Cuirass and stripped bare ([Debug] FitDump,
// tools/hand-scripts/fit-dump-*.txt, tests/data/fit-*.txt), fitted here as
// the game fits them and measured as the game's "Contact: mesh gap" line
// measures - over each capsule's whole reachable surface. Nadi, 2026-11-07:
// "wie können wir die genauigkeit verbessern? am ende sollten die tests
// passen".

#include <cstdio>
#include <string>
#include <vector>

#include "game/CapsuleAccuracy.h"
#include "game/FitDump.h"

using namespace obvr::game;
using obvr::NiPoint3;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

struct Outfit {
	const char* file;
	const char* what;
	float wantShare;  // of the samples within 3 units of the mesh
	float wantP95;    // the 95th percentile of the gap, units
	float wantGame;   // within 3 units as the game measures, against every vertex
};

void Measure(const std::string& dir, const Outfit& o, float wantShare, float wantP95) {
	FitDumpData d;
	const std::string path = dir + "/" + o.file;
	if (!ReadFitDump(path.c_str(), d)) {
		Check(false, (std::string("read ") + path).c_str());
		return;
	}
	FittedCapsule caps[kFitCapsulesMax];
	const UInt32 n = FitBodyCapsules(d.bones.data(), static_cast<UInt32>(d.bones.size()), d.vertices.data(),
	                                 static_cast<UInt32>(d.vertices.size()), caps, kFitCapsulesMax);
	const std::vector<PlacedCapsule> placed = PlaceCapsules(d.bones.data(), caps, n);
	std::vector<NiPoint3> world;
	std::vector<UInt8> groupOf;
	for (const FitVertex& v : d.vertices) {
		world.push_back(v.bind);
		groupOf.push_back(v.bone < d.bones.size() ? FitGroupOfBone(d.bones[v.bone].name) : kFitNoGroup);
	}
	std::printf("%s: %u vertices, %u capsules\n", o.what, static_cast<unsigned>(world.size()), n);
	for (UInt8 g = 0; g < kFitGroupCount; ++g) {
		const CapsuleAccuracy a = MeasureCapsuleAccuracy(placed, world.data(), groupOf.data(), static_cast<UInt32>(world.size()), g);
		if (a.samples == 0) {
			continue;
		}
		std::printf("        %-16s %4u samples, %5.1f %% within 3, p95 %5.1f, worst over %5.1f into %5.1f\n",
		            FitGroupInfoOf(g).name, a.samples, 100.0 * a.Share(), static_cast<double>(a.p95),
		            static_cast<double>(a.worstOver), static_cast<double>(a.worstInto));
	}
	const CapsuleAccuracy all = MeasureCapsuleAccuracy(placed, world.data(), groupOf.data(), static_cast<UInt32>(world.size()));
	// as the game measures it: against every vertex, a neighbour's too (a
	// cap reaching over the waist meets the waist's skin)
	const CapsuleAccuracy game = MeasureCapsuleAccuracy(placed, world.data(), nullptr, static_cast<UInt32>(world.size()));
	std::printf("        against every vertex (the game's measure): %.1f %% within 3, p95 %.1f\n", 100.0 * game.Share(),
	            static_cast<double>(game.p95));
	char line[256];
	std::snprintf(line, sizeof(line), "%s: %.1f %% of %u samples within 3 units (wanted %.0f %%), p95 %.1f (wanted <= %.1f)",
	              o.what, 100.0 * all.Share(), all.samples, 100.0 * wantShare, static_cast<double>(all.p95),
	              static_cast<double>(wantP95));
	Check(all.samples > 100 && all.Share() >= wantShare && all.p95 <= wantP95, line);
	std::snprintf(line, sizeof(line), "%s, as the game measures: %.1f %% within 3 units (wanted %.0f %%)", o.what,
	              100.0 * game.Share(), 100.0 * o.wantGame);
	Check(game.samples > 100 && game.Share() >= o.wantGame, line);
}

}  // namespace

int main(int argc, char** argv) {
	const std::string dir = argc > 1 ? argv[1] : OBVR_TEST_DATA_DIR;
	const Outfit outfits[] = {
		// measured 2026-11-07 after RefineFit and the ends judged against the
		// neighbours (FitBestEnds' context), kept as a ratchet: the robe's skirt
		// (a sheet round both legs, skinned half to each) fits worst; as the
		// game measures, against every vertex, 81 / 87 / 91 % (78 / 83 / 89
		// before the context)
		{"fit-jauffre-robe.txt", "Jauffre in his robe (vanilla clothes)", 0.85f, 5.0f, 0.80f},
		{"fit-jauffre-dwarven.txt", "Jauffre in a mod's Dwarven Cuirass", 0.92f, 3.6f, 0.86f},
		{"fit-jauffre-bare.txt", "Jauffre bare (a body replacer)", 0.95f, 3.0f, 0.90f},
	};
	for (const Outfit& o : outfits) {
		Measure(dir, o, o.wantShare, o.wantP95);
	}
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("All capsule accuracy checks passed\n");
	return 0;
}
