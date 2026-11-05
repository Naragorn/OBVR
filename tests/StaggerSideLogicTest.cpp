// The side a stagger falls to (game/StaggerSideLogic.h): every flow of the
// file names' lean, the blow's side, the pick and the skeleton gate.
#include <cstdio>
#include <limits>

#include "game/StaggerSideLogic.h"

namespace {

using namespace obvr::game;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

}  // namespace

int main() {
	std::printf("The side a stagger falls to\n");

	// The files and their lean (+1 the actor's right, -1 its left).
	Check(StaggerFileLean("Characters\\_Male\\OneHandStagger.kf") == 1, "the one-handed stagger leans right");
	Check(StaggerFileLean("characters\\_male\\twohandstagger.kf") == 1, "the two-handed stagger leans right");
	Check(StaggerFileLean("Characters/_Male/StaffStagger.kf") == 1, "the staff's stagger leans right");
	Check(StaggerFileLean("Characters\\_Male\\HandToHandStagger.kf") == -1, "the fists' stagger leans left");
	Check(StaggerFileLean("Characters\\_Male\\SpecialAnims\\OneHandStagger_OBVRMirror.kf") == -1,
	      "the one-handed mirror leans left");
	Check(StaggerFileLean("Characters\\_Male\\SpecialAnims\\HandToHandStagger_OBVRMirror.kf") == 1,
	      "the fists' mirror leans right");
	Check(StaggerFileLean("Characters\\_Male\\SwimStagger.kf") == 0, "the swimming stagger: no side known");
	Check(StaggerFileLean("Characters\\_Male\\OneHandAttackLeft.kf") == 0, "an attack is no stagger");
	Check(StaggerFileLean(nullptr) == 0, "no name: no side");
	Check(IsStaggerMirror("x\\OneHandStagger_obvrmirror.KF") && !IsStaggerMirror("x\\OneHandStagger.kf"),
	      "the mirror is told by its tag, case aside");

	// The blow's side in the actor's frame: the world vector's part along the
	// actor's right, (cos z, -sin z).
	Check(Near(BlowLateral(1.0f, 0.0f, 0.0f), 1.0f), "facing +y, a blow along +x goes to its right");
	Check(Near(BlowLateral(-0.5f, 0.0f, 0.0f), -0.5f), "facing +y, a blow along -x goes to its left");
	Check(Near(BlowLateral(0.0f, 1.0f, 1.5707963f), -1.0f), "facing +x, a blow along +y goes to its left");
	Check(Near(BlowLateral(0.0f, 2.0f, 0.0f), 0.0f), "a blow straight along the facing has no side");

	// The pick among the group's files.
	const char* both[] = {"Characters\\_Male\\OneHandStagger.kf",
	                      "Characters\\_Male\\SpecialAnims\\OneHandStagger_OBVRMirror.kf"};
	Check(PickStaggerEntry(both, 2, 0.3f, 0.05f) == 0, "a blow to the right: the vanilla file, which leans right");
	Check(PickStaggerEntry(both, 2, -0.3f, 0.05f) == 1, "a blow to the left: the mirror");
	Check(PickStaggerEntry(both, 2, 0.02f, 0.05f) == -1, "a blow with too little side: the engine's draw");
	Check(PickStaggerEntry(both, 2, -0.02f, 0.05f) == -1, "and the same the other way");
	const char* mirrorFirst[] = {"SpecialAnims\\OneHandStagger_OBVRMirror.kf", "OneHandStagger.kf"};
	Check(PickStaggerEntry(mirrorFirst, 2, 0.3f, 0.05f) == 1, "the order of the entries does not matter");
	const char* fists[] = {"HandToHandStagger.kf", "SpecialAnims\\HandToHandStagger_OBVRMirror.kf"};
	Check(PickStaggerEntry(fists, 2, 0.3f, 0.05f) == 1, "the fists: a blow to the right takes the mirror");
	Check(PickStaggerEntry(fists, 2, -0.3f, 0.05f) == 0, "and a blow to the left the vanilla file");
	const char* lone[] = {"OneHandStagger.kf"};
	Check(PickStaggerEntry(lone, 1, -0.3f, 0.05f) == -1, "no file of the wanted side: the engine's draw");
	const char* unknown[] = {"SwimStagger.kf", "Other.kf"};
	Check(PickStaggerEntry(unknown, 2, 0.3f, 0.05f) == -1, "files of no known side: the engine's draw");
	const char* withNull[] = {nullptr, "SpecialAnims\\OneHandStagger_OBVRMirror.kf"};
	Check(PickStaggerEntry(withNull, 2, -0.3f, 0.05f) == 1, "an entry without a name is passed over");
	Check(PickStaggerEntry(nullptr, 0, 0.3f, 0.05f) == -1, "no entries: the engine's draw");
	Check(PickStaggerEntry(both, 2, std::numeric_limits<float>::quiet_NaN(), 0.05f) == -1, "a side that is not a number: the engine's draw");

	// Which NPCs get the mirrors: those whose skeleton the files were made for.
	Check(SkeletonTakesStaggerMirrors("Characters\\_Male\\skeleton.nif"), "the vanilla skeleton takes them");
	Check(SkeletonTakesStaggerMirrors("characters\\_male\\SkeletonBeast.nif"), "the beast skeleton beside it too");
	Check(SkeletonTakesStaggerMirrors("Characters/_Male/skeleton.nif"), "forward slashes alike");
	Check(!SkeletonTakesStaggerMirrors("Characters\\Sheogorath\\skeleton.nif"), "a skeleton of its own folder does not");
	Check(!SkeletonTakesStaggerMirrors("Creatures\\Skeleton\\Skeleton.nif"), "a creature's does not");
	Check(!SkeletonTakesStaggerMirrors(""), "an empty path does not");
	Check(!SkeletonTakesStaggerMirrors(nullptr), "no path does not");

	// The names OBVR adds to an NPC's list, and the check that keeps it once.
	Check(kStaggerMirrorFileCount == 4, "four stances have a mirror");
	bool allTagged = true;
	for (UInt32 i = 0; i < kStaggerMirrorFileCount; ++i) {
		allTagged = allTagged && IsStaggerMirror(kStaggerMirrorFiles[i]) && StaggerFileLean(kStaggerMirrorFiles[i]) != 0;
	}
	Check(allTagged, "each added name is a mirror of a known side");
	Check(SameAnimName("OneHandStagger_OBVRMirror.kf", "onehandstagger_obvrmirror.KF"), "names compare without case");
	Check(!SameAnimName("OneHandStagger_OBVRMirror.kf", "OneHandStagger.kf"), "different names differ");
	Check(!SameAnimName(nullptr, "a.kf") && !SameAnimName("a.kf", nullptr), "no name is no match");

	// The stub and the site's patch, byte by byte.
	UInt8 stub[32] = {};
	const UInt32 stubAt = 0x10000000;
	const UInt32 pickAt = 0x10001000;
	Check(BuildStaggerPickStub(stub, sizeof(stub), stubAt, pickAt) == kStaggerPickStubLength, "the stub's length");
	const UInt8 want[] = {0x51, 0x51, 0xE8, 0, 0, 0, 0, 0x83, 0xC4, 0x04, 0x59, 0x8B, 0x11, 0x50, 0xFF, 0x52, 0x10, 0xC3};
	bool same = true;
	for (UInt32 i = 0; i < sizeof(want); ++i) {
		same = same && (i >= 3 && i < 7 ? true : stub[i] == want[i]);
	}
	Check(same, "the stub's bytes: save, ask, restore, call with the index, return");
	const UInt32 rel = stub[3] | (stub[4] << 8) | (stub[5] << 16) | (static_cast<UInt32>(stub[6]) << 24);
	Check(stubAt + 7 + rel == pickAt, "the stub's call reaches the pick");
	Check(BuildStaggerPickStub(stub, kStaggerPickStubLength - 1, stubAt, pickAt) == 0, "a short buffer: no stub");
	Check(BuildStaggerPickStub(nullptr, 64, stubAt, pickAt) == 0, "no buffer: no stub");
	UInt8 site[8] = {};
	Check(BuildStaggerPickSitePatch(site, sizeof(site), stubAt) == 7, "the site's patch is the seven bytes it replaces");
	const UInt32 siteRel = site[1] | (site[2] << 8) | (site[3] << 16) | (static_cast<UInt32>(site[4]) << 24);
	Check(site[0] == 0xE8 && kStaggerPickSite + 5 + siteRel == stubAt, "the site calls the stub");
	Check(site[5] == 0x90 && site[6] == 0x90, "and two nops fill it, so the return lands on 0x00476291");
	Check(BuildStaggerPickSitePatch(site, 6, stubAt) == 0, "a short buffer: no patch");

	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
