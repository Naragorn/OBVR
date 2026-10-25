// Checks the capsules fitted to the meshes an actor wears
// (game/CapsuleFitLogic.h, docs/mesh-fitted-capsules-spec.md 4.2 and 6):
// a cylinder's radius and ends, a stray vertex, a bone off its flesh, a flat
// section and its split, a bone without a child, a short blob, no vertices;
// the bones' groups and parts; the whole fit over a made-up skeleton - the
// frame bone, its fallback, the axis to the child, the torso's split and its
// refusal, too few vertices - and a fitted capsule placed through a bone's
// transform and scale.

#include <cmath>
#include <cstdio>
#include <vector>

#include "game/CapsuleFitLogic.h"

using obvr::NiMatrix33;
using obvr::NiPoint3;
using obvr::NiTransform;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float eps = 0.001f) { return a - b < eps && b - a < eps; }

constexpr float kPi = 3.14159265358979f;

// Rings of points round the x axis from x0 to x1: `radius` along y,
// `radius * squash` along z, the middle at (cy, cz).
std::vector<NiPoint3> Rings(float radius, float x0, float x1, float squash = 1.0f, float cy = 0.0f, float cz = 0.0f,
                            int rings = 21, int perRing = 24) {
	std::vector<NiPoint3> pts;
	for (int i = 0; i < rings; ++i) {
		const float x = x0 + (x1 - x0) * static_cast<float>(i) / static_cast<float>(rings - 1);
		for (int k = 0; k < perRing; ++k) {
			const float a = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(perRing);
			pts.push_back(NiPoint3{x, cy + radius * std::cos(a), cz + radius * squash * std::sin(a)});
		}
	}
	return pts;
}

const NiPoint3 kOrigin{0.0f, 0.0f, 0.0f};
const NiPoint3 kAlongX{1.0f, 0.0f, 0.0f};

void TestFit() {
	std::printf("The fit of one capsule\n");
	FitShape f;
	const std::vector<NiPoint3> cyl = Rings(5.0f, 0.0f, 30.0f);
	Check(FitCapsuleToPoints(cyl.data(), static_cast<UInt32>(cyl.size()), &kOrigin, &kAlongX, f), "a cylinder fits");
	Check(Near(f.radius, 5.0f, 0.01f), "a cylinder's radius");
	Check(Near(f.a.x, 5.0f, 0.01f) && Near(f.b.x, 25.0f, 0.01f), "its ends: the extent trimmed by the radius");
	Check(f.flatness < 1.01f, "a round section is not flat");

	std::vector<NiPoint3> stray = cyl;
	stray.push_back(NiPoint3{15.0f, 50.0f, 0.0f});
	FitCapsuleToPoints(stray.data(), static_cast<UInt32>(stray.size()), &kOrigin, &kAlongX, f);
	Check(Near(f.radius, 5.0f, 0.05f), "a stray vertex does not fatten it");

	const std::vector<NiPoint3> off = Rings(5.0f, 0.0f, 30.0f, 1.0f, 4.0f, 0.0f);
	FitCapsuleToPoints(off.data(), static_cast<UInt32>(off.size()), &kOrigin, &kAlongX, f);
	Check(Near(f.radius, 5.0f, 0.01f) && Near(f.a.y, 4.0f, 0.01f) && Near(f.b.y, 4.0f, 0.01f),
	      "a bone off its flesh: the line moves onto the flesh, the radius stays");

	const std::vector<NiPoint3> flat = Rings(10.0f, 0.0f, 30.0f, 0.5f);
	FitCapsuleToPoints(flat.data(), static_cast<UInt32>(flat.size()), &kOrigin, &kAlongX, f);
	Check(Near(f.flatness, 2.0f, 0.05f), "a section twice as wide as deep reads 2 flat");
	Check(Near(std::fabs(f.major.y), 1.0f, 0.01f), "its major direction is the wide one");
	FitShape two[2];
	Check(SplitFittedCapsule(flat.data(), static_cast<UInt32>(flat.size()), f, two), "a flat section splits");
	Check(two[0].radius < 0.75f * f.radius && two[1].radius < 0.75f * f.radius, "into two rounder capsules");
	Check(Near(two[0].radius, two[1].radius, 0.05f), "both alike for a symmetric section");
	Check((two[0].a.y < 0.0f) != (two[1].a.y < 0.0f), "one on each side");

	const NiPoint3 d{0.70710678f, 0.70710678f, 0.0f};
	std::vector<NiPoint3> diag;
	for (int i = -40; i <= 40; ++i) {
		const float t = 0.5f * static_cast<float>(i);
		diag.push_back(NiPoint3{d.x * t, d.y * t, 0.5f * std::sin(t)});
	}
	FitCapsuleToPoints(diag.data(), static_cast<UInt32>(diag.size()), nullptr, nullptr, f);
	Check(Near(std::fabs(f.axis.x * d.x + f.axis.y * d.y + f.axis.z * d.z), 1.0f, 0.001f),
	      "without a child bone: the principal axis");

	const std::vector<NiPoint3> blob = Rings(5.0f, 0.0f, 6.0f);
	FitCapsuleToPoints(blob.data(), static_cast<UInt32>(blob.size()), &kOrigin, &kAlongX, f);
	Check(Near(f.a.x, 3.0f, 0.01f) && Near(f.b.x, 3.0f, 0.01f), "shorter than its width: a sphere in the middle");

	Check(!FitCapsuleToPoints(nullptr, 0, &kOrigin, &kAlongX, f), "no vertices: no capsule");
}

void TestGroups() {
	std::printf("The bones' groups and parts\n");
	Check(FitGroupOfBone("Bip01 Head") == kFitHead, "the head");
	Check(FitGroupOfBone("Bip01 Neck") == kFitNeck && FitGroupOfBone("Bip01 Neck1") == kFitNeck, "both neck bones");
	Check(FitGroupOfBone("Bip01 Spine") == kFitTorso && FitGroupOfBone("Bip01 Spine1") == kFitTorso &&
	          FitGroupOfBone("Bip01 Spine2") == kFitTorso,
	      "the three spine bones: one torso");
	Check(FitGroupOfBone("Bip01 Pelvis") == kFitPelvis, "the pelvis");
	Check(FitGroupOfBone("Bip01 L UpperArmTwist") == kFitLUpperArm && FitGroupOfBone("Bip01 L UpperArm") == kFitLUpperArm,
	      "a twist bone with its limb");
	Check(FitGroupOfBone("Bip01 R ForearmTwist") == kFitRForearm, "the right forearm's twist");
	Check(FitGroupOfBone("Bip01 L Finger02") == kFitLHand && FitGroupOfBone("Bip01 R Finger4") == kFitRHand,
	      "fingers with the hand");
	Check(FitGroupOfBone("Bip01 R Toe0") == kFitRFoot, "toes with the foot");
	Check(FitGroupOfBone("Bip01 L Clavicle") == kFitLClavicle, "a clavicle");
	Check(FitGroupOfBone("Bip01 R Thigh") == kFitRThigh && FitGroupOfBone("Bip01 R Calf") == kFitRCalf, "the leg");
	Check(FitGroupOfBone("ChinBone01") == kFitNoGroup && FitGroupOfBone("Bip01 L Wrist Helper") == kFitNoGroup &&
	          FitGroupOfBone("Bip01") == kFitNoGroup && FitGroupOfBone(nullptr) == kFitNoGroup,
	      "a bone of no group: none");
	Check(FitGroupInfoOf(kFitHead).part == BodyPart::Head && FitGroupInfoOf(kFitLForearm).part == BodyPart::LeftArm &&
	          FitGroupInfoOf(kFitRCalf).part == BodyPart::RightLeg && FitGroupInfoOf(kFitTorso).part == BodyPart::Torso,
	      "each group names its body part");
	Check(FitGroupInfoOf(kFitTorso).splitWhenFlat && !FitGroupInfoOf(kFitLThigh).splitWhenFlat,
	      "only the torso splits when flat");
}

NiTransform Translation(float x, float y, float z) {
	NiTransform t;
	t.rot = NiMatrix33::Identity();
	t.pos = NiPoint3{x, y, z};
	t.scale = 1.0f;
	return t;
}

// A skin-to-bone for a bone whose bind origin sits at `at` with the
// skeleton's axes: the inverse of its bind, a translation by -at.
FitBone Bone(const char* name, float x, float y, float z) { return FitBone{name, Translation(-x, -y, -z)}; }

void Add(std::vector<FitVertex>& to, const std::vector<NiPoint3>& pts, UInt16 bone, const NiPoint3& shift) {
	for (const NiPoint3& p : pts) {
		to.push_back(FitVertex{p + shift, bone});
	}
}

const FittedCapsule* FindGroup(const FittedCapsule* caps, UInt32 n, UInt8 group, UInt32 nth = 0) {
	for (UInt32 i = 0; i < n; ++i) {
		if (caps[i].group == group && nth-- == 0) {
			return &caps[i];
		}
	}
	return nullptr;
}

void TestBody() {
	std::printf("The whole fit over a made-up skeleton\n");
	// An upper arm along x from (10,0,0) to its forearm at (40,0,0), its
	// twist halfway; a hand at (60,0,0) with no child; a torso up z from the
	// spine at (0,0,100) to the neck at (0,0,130), twice as wide (x) as deep.
	std::vector<FitBone> bones = {
		Bone("Bip01 L UpperArm", 10.0f, 0.0f, 0.0f), Bone("Bip01 L UpperArmTwist", 25.0f, 0.0f, 0.0f),
		Bone("Bip01 L Forearm", 40.0f, 0.0f, 0.0f),  Bone("Bip01 L Hand", 60.0f, 0.0f, 0.0f),
		Bone("Bip01 Spine", 0.0f, 0.0f, 100.0f),     Bone("Bip01 Spine1", 0.0f, 0.0f, 110.0f),
		Bone("Bip01 Neck1", 0.0f, 0.0f, 130.0f),
	};
	std::vector<FitVertex> v;
	// the upper arm's flesh, owned half by the bone, half by its twist
	const std::vector<NiPoint3> arm = Rings(5.0f, 0.0f, 30.0f);
	for (UInt32 i = 0; i < arm.size(); ++i) {
		v.push_back(FitVertex{arm[i] + NiPoint3{10.0f, 0.0f, 0.0f}, static_cast<UInt16>(i % 2 == 0 ? 0 : 1)});
	}
	// a hand: a blob along y, its principal axis
	std::vector<NiPoint3> hand;
	for (int i = -10; i <= 10; ++i) {
		for (int k = 0; k < 12; ++k) {
			const float a = 2.0f * kPi * static_cast<float>(k) / 12.0f;
			hand.push_back(NiPoint3{2.0f * std::cos(a), static_cast<float>(i), 2.0f * std::sin(a)});
		}
	}
	Add(v, hand, 3, NiPoint3{60.0f, 0.0f, 0.0f});
	// a torso up z: rings round z, 12 wide in x and 6 deep in y
	std::vector<NiPoint3> torso;
	for (int i = 0; i < 21; ++i) {
		const float z = 100.0f + 30.0f * static_cast<float>(i) / 20.0f;
		for (int k = 0; k < 24; ++k) {
			const float a = 2.0f * kPi * static_cast<float>(k) / 24.0f;
			torso.push_back(NiPoint3{12.0f * std::cos(a), 6.0f * std::sin(a), z});
		}
	}
	for (UInt32 i = 0; i < torso.size(); ++i) {
		v.push_back(FitVertex{torso[i], static_cast<UInt16>(i % 2 == 0 ? 4 : 5)});
	}

	FittedCapsule caps[kFitCapsulesMax];
	const UInt32 n = FitBodyCapsules(bones.data(), static_cast<UInt32>(bones.size()), v.data(),
	                                 static_cast<UInt32>(v.size()), caps, kFitCapsulesMax);
	const FittedCapsule* upper = FindGroup(caps, n, kFitLUpperArm);
	Check(upper != nullptr && upper->vertices == arm.size(), "the twist's vertices join their limb's group");
	Check(upper != nullptr && upper->frameBone == 0, "the group's frame is its named bone");
	Check(upper != nullptr && Near(upper->radius, 5.0f, 0.01f), "the upper arm's radius");
	Check(upper != nullptr && Near(upper->a.x, 5.0f, 0.01f) && Near(upper->b.x, 25.0f, 0.01f) &&
	          Near(upper->a.y, 0.0f, 0.01f) && Near(upper->a.z, 0.0f, 0.01f),
	      "its ends in the upper arm's own space, along the axis to the forearm");
	Check(upper != nullptr && upper->part == BodyPart::LeftArm, "its part");
	const FittedCapsule* handCap = FindGroup(caps, n, kFitLHand);
	Check(handCap != nullptr && Near(std::fabs(handCap->b.y - handCap->a.y), 2.0f * 10.0f - 2.0f * handCap->radius, 0.05f),
	      "the hand, with no child bone, along its vertices' principal axis");
	const FittedCapsule* t0 = FindGroup(caps, n, kFitTorso, 0);
	const FittedCapsule* t1 = FindGroup(caps, n, kFitTorso, 1);
	Check(t0 != nullptr && t1 != nullptr, "the flat torso splits in two");
	Check(t0 != nullptr && t1 != nullptr && t0->radius < 9.0f && t1->radius < 9.0f && Near(t0->radius, t1->radius, 0.1f),
	      "two alike, rounder than one");
	Check(t0 != nullptr && t0->frameBone == 4, "the torso's frame is the spine");
	Check(FindGroup(caps, n, kFitLForearm) == nullptr, "a group with no vertices: no capsule");

	// a round torso stays one; a group's named frame bone missing takes a
	// member's
	std::vector<FitBone> round = {Bone("Bip01 Spine1", 0.0f, 0.0f, 110.0f), Bone("Bip01 Neck1", 0.0f, 0.0f, 130.0f)};
	std::vector<FitVertex> rv;
	for (int i = 0; i < 21; ++i) {
		const float z = 110.0f + 20.0f * static_cast<float>(i) / 20.0f;
		for (int k = 0; k < 24; ++k) {
			const float a = 2.0f * kPi * static_cast<float>(k) / 24.0f;
			rv.push_back(FitVertex{NiPoint3{8.0f * std::cos(a), 8.0f * std::sin(a), z}, 0});
		}
	}
	FittedCapsule rc[kFitCapsulesMax];
	const UInt32 rn = FitBodyCapsules(round.data(), static_cast<UInt32>(round.size()), rv.data(),
	                                  static_cast<UInt32>(rv.size()), rc, kFitCapsulesMax);
	Check(rn == 1 && rc[0].group == kFitTorso, "a round torso: one capsule");
	Check(rn == 1 && rc[0].frameBone == 0, "the spine missing: the frame falls to a member of the group");

	// too few vertices
	std::vector<FitVertex> few(rv.begin(), rv.begin() + (kFitMinVertices - 1));
	Check(FitBodyCapsules(round.data(), static_cast<UInt32>(round.size()), few.data(),
	                      static_cast<UInt32>(few.size()), rc, kFitCapsulesMax) == 0,
	      "fewer vertices than the least: no capsule");
	Check(FitBodyCapsules(round.data(), static_cast<UInt32>(round.size()), rv.data(), static_cast<UInt32>(rv.size()), rc,
	                      0) == 0,
	      "no room: none");
}

void TestPlace() {
	std::printf("A fitted capsule placed by its bone\n");
	FittedCapsule c;
	c.a = NiPoint3{5.0f, 0.0f, 0.0f};
	c.b = NiPoint3{25.0f, 0.0f, 0.0f};
	c.radius = 4.0f;
	NiTransform world;
	world.rot = NiMatrix33::Identity();
	// turned 90 degrees about z: x to y
	world.rot.data[0][0] = 0.0f;
	world.rot.data[0][1] = -1.0f;
	world.rot.data[1][0] = 1.0f;
	world.rot.data[1][1] = 0.0f;
	world.pos = NiPoint3{100.0f, 200.0f, 300.0f};
	world.scale = 1.5f;
	NiPoint3 a, b;
	float r = 0.0f;
	PlaceFittedCapsule(world, c, a, b, r);
	Check(Near(a.x, 100.0f) && Near(a.y, 207.5f) && Near(a.z, 300.0f), "an end through the rotation, scale and place");
	Check(Near(b.y, 237.5f), "the other end");
	Check(Near(r, 6.0f), "the radius times the scale");
}

void TestPrint() {
	std::printf("The outfit's print\n");
	const UInt32 a[] = {0x1000, 475, 0x2000, 410};
	const UInt32 b[] = {0x1000, 475, 0x2000, 411};
	const UInt32 c[] = {0x1000, 475};
	Check(OutfitPrint(a, 4) == OutfitPrint(a, 4), "the same shapes: the same print");
	Check(OutfitPrint(a, 4) != OutfitPrint(b, 4), "a count changed: another print");
	Check(OutfitPrint(a, 4) != OutfitPrint(c, 2), "a shape fewer: another print");
	Check(OutfitPrint(nullptr, 0) != 0, "nothing worn still prints, never as 'not read'");
}

}  // namespace

int main() {
	TestFit();
	TestGroups();
	TestBody();
	TestPlace();
	TestPrint();
	if (g_failures > 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
