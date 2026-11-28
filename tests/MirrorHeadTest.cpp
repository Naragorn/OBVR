// Checks the player's head in the mirror (game/MirrorHeadLogic.h,
// docs/mirror-spec.md stage 3): the plan of what the mirror's render switches
// and puts back - every refusal in its order, the bits lifted only where this
// code set them, the skins' list with its duplicates and its overflow, the
// stale stamp.

#include <cstdio>
#include <limits>

#include "game/MirrorHeadLogic.h"

using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

// A frame with the body drawn and the head collapsed, both FaceGen bits ours,
// three skins on the head's bones.
MirrorHeadInputs Collapsed() {
	MirrorHeadInputs in;
	in.bodyRead = true;
	in.bodyDrawn = true;
	in.headRead = true;
	in.headWorldScale = 0.0f;
	in.headLocalScale = 1.0f;
	in.cullOurs[0] = true;
	in.cullOurs[1] = true;
	in.skins = 3;
	return in;
}

bool NothingSwitched(const MirrorHeadPlan& p) {
	return !p.restoreHead && !p.liftCull[0] && !p.liftCull[1] && p.staleSkins == 0;
}

void TestShown() {
	std::printf("The head collapsed for the eyes: shown for the mirror\n");
	const MirrorHeadPlan p = PlanMirrorHead(Collapsed());
	Check(p.refusal == MirrorHeadRefusal::None, "no refusal");
	Check(p.restoreHead, "the head's true transform for the render, the collapse after");
	Check(p.liftCull[0] && p.liftCull[1], "both FaceGen bits this code set come off");
	Check(p.staleSkins == 3, "the three skins on the head's bones are built again");

	MirrorHeadInputs theirs = Collapsed();
	theirs.cullOurs[0] = false;  // the skinned face's bit was someone else's
	const MirrorHeadPlan q = PlanMirrorHead(theirs);
	Check(q.refusal == MirrorHeadRefusal::None && !q.liftCull[0] && q.liftCull[1],
	      "a bit someone else set stays on; ours comes off");

	MirrorHeadInputs noCull = Collapsed();
	noCull.cullOurs[0] = noCull.cullOurs[1] = false;  // [Body] CullHiddenHead=0
	const MirrorHeadPlan r = PlanMirrorHead(noCull);
	Check(r.refusal == MirrorHeadRefusal::None && r.restoreHead && !r.liftCull[0] && !r.liftCull[1],
	      "CullHiddenHead=0: only the transform and the skins");

	MirrorHeadInputs noSkins = Collapsed();
	noSkins.skins = 0;
	const MirrorHeadPlan s = PlanMirrorHead(noSkins);
	Check(s.refusal == MirrorHeadRefusal::None && s.restoreHead && s.staleSkins == 0,
	      "no skin found on the head: the transform and the bits still switch");
}

void TestRefusals() {
	std::printf("Every refusal switches nothing, in its order\n");
	MirrorHeadInputs in = Collapsed();
	in.bodyRead = false;
	in.bodyDrawn = false;
	in.headRead = false;
	MirrorHeadPlan p = PlanMirrorHead(in);
	Check(p.refusal == MirrorHeadRefusal::NoBody && NothingSwitched(p), "no body root: nothing (first)");

	in = Collapsed();
	in.bodyDrawn = false;
	in.headRead = false;
	p = PlanMirrorHead(in);
	Check(p.refusal == MirrorHeadRefusal::BodyHidden && NothingSwitched(p),
	      "[Body] Visible=0 or the body away: hands and room only, before the head is asked");

	in = Collapsed();
	in.headRead = false;
	in.headLocalScale = -1.0f;
	p = PlanMirrorHead(in);
	Check(p.refusal == MirrorHeadRefusal::NoHead && NothingSwitched(p), "no head bone: nothing, before its scale");

	in = Collapsed();
	in.headWorldScale = 1.0f;
	in.headLocalScale = -1.0f;
	p = PlanMirrorHead(in);
	Check(p.refusal == MirrorHeadRefusal::AlreadyShown && NothingSwitched(p),
	      "[Body] HideHead=0 (the head not collapsed): nothing to switch, before the scale's check");

	in = Collapsed();
	in.headWorldScale = std::numeric_limits<float>::quiet_NaN();
	p = PlanMirrorHead(in);
	Check(p.refusal == MirrorHeadRefusal::AlreadyShown && NothingSwitched(p),
	      "a world scale of no number is no collapse this code made: left alone");

	in = Collapsed();
	in.headLocalScale = 0.0f;
	in.skinsOverflow = true;
	p = PlanMirrorHead(in);
	Check(p.refusal == MirrorHeadRefusal::BadScale && NothingSwitched(p), "a local scale of 0: no true transform to give");
	in.headLocalScale = std::numeric_limits<float>::quiet_NaN();
	p = PlanMirrorHead(in);
	Check(p.refusal == MirrorHeadRefusal::BadScale && NothingSwitched(p), "a local scale of no number: refused");
	in.headLocalScale = std::numeric_limits<float>::infinity();
	p = PlanMirrorHead(in);
	Check(p.refusal == MirrorHeadRefusal::BadScale && NothingSwitched(p), "an endless local scale: refused");

	in = Collapsed();
	in.skinsOverflow = true;
	p = PlanMirrorHead(in);
	Check(p.refusal == MirrorHeadRefusal::TooManySkins && NothingSwitched(p),
	      "more skins than the list holds: the head stays hidden rather than half shown");
	Check(MirrorHeadRefusalName(MirrorHeadRefusal::None)[0] == 's' &&
	          MirrorHeadRefusalName(MirrorHeadRefusal::TooManySkins)[0] == 't' &&
	          MirrorHeadRefusalName(MirrorHeadRefusal::Count)[0] == 'u',
	      "each refusal has its name for the log");
}

void TestSkinList() {
	std::printf("The skins on the head's bones\n");
	MirrorHeadSkins list;
	AddMirrorHeadSkin(list, 0);
	Check(list.count == 0 && !list.overflow, "a null skin is no skin");
	AddMirrorHeadSkin(list, 0x1000);
	AddMirrorHeadSkin(list, 0x2000);
	AddMirrorHeadSkin(list, 0x1000);
	Check(list.count == 2 && list.skins[0] == 0x1000 && list.skins[1] == 0x2000, "a skin shared by two shapes is kept once");
	MirrorHeadSkins full;
	for (std::uintptr_t i = 1; i <= kMirrorHeadSkinsMax; ++i) {
		AddMirrorHeadSkin(full, i * 16);
	}
	Check(full.count == kMirrorHeadSkinsMax && !full.overflow, "the list holds its size");
	AddMirrorHeadSkin(full, 16);
	Check(!full.overflow, "one already in a full list is no overflow");
	AddMirrorHeadSkin(full, 0xFFFF0);
	Check(full.overflow && full.count == kMirrorHeadSkinsMax, "one more: overflow, the count stays");
}

void TestStaleStamp() {
	std::printf("The stale stamp\n");
	Check(StaleMirrorStamp(5u) != 5u, "never the stamp it was");
	Check(StaleMirrorStamp(StaleMirrorStamp(0x12345678u)) == 0x12345678u, "turned twice it comes back (a single bit)");
	Check(StaleMirrorStamp(0u) == 0x80000000u && StaleMirrorStamp(0x80000001u) == 1u, "the top bit turned");
}

}  // namespace

int main() {
	TestShown();
	TestRefusals();
	TestSkinList();
	TestStaleStamp();
	if (g_failures > 0) {
		std::printf("%d failing check(s)\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
