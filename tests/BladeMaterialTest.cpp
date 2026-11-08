// What a blade meets on a body (game/BladeMaterial.h): the material of a
// shape by the worn item it hangs under, a worn item node's form ID, the
// armour's points, the material most vertices have; carried through the
// mesh gap (the vertex that is the surface), into a radial field's bins
// (game/RadialFieldBuild.h) and read back at a point (FieldMaterialAt,
// BodyMaterialAt).

#include <cstdio>
#include <vector>

#include "game/BladeMaterial.h"
#include "game/BodyShapes.h"
#include "game/RadialFieldBuild.h"

using namespace obvr::game;
using obvr::NiPoint3;
namespace math = obvr::math;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

void TestWorn() {
	std::printf("the material of a worn item\n");
	Check(MaterialOfWorn(0, false, 0, false) == BodyMaterial::Skin, "under no worn item: the body's skin");
	Check(MaterialOfWorn(0x1C6D1, true, kWornTypeArmor, false) == BodyMaterial::Light, "armour without the flag: light");
	Check(MaterialOfWorn(0x1C6D1, true, kWornTypeArmor, true) == BodyMaterial::Heavy, "armour with the flag: heavy");
	Check(MaterialOfWorn(0x1C6D1, true, kWornTypeClothing, true) == BodyMaterial::Cloth,
	      "clothing: cloth, whatever its flags say");
	Check(MaterialOfWorn(0x907, true, 0x09, false) == BodyMaterial::Skin, "another kind of form (the race's body): skin");
	Check(MaterialOfWorn(0x1C6D1, false, kWornTypeArmor, true) == BodyMaterial::Unknown, "the form not read: unknown");
	Check(BodyMaterialName(BodyMaterial::Heavy)[0] == 'h' && BodyMaterialName(static_cast<BodyMaterial>(9))[0] == 'u',
	      "names for the log, unknown for anything else");
}

void TestNames() {
	std::printf("a worn item node's form ID\n");
	Check(WornItemIdOf("UpperBody (0001C6D1)") == 0x0001C6D1u, "slot, a space, the ID in brackets");
	Check(WornItemIdOf("Hand  (000661c2)") == 0x000661C2u, "two spaces and lower case");
	Check(WornItemIdOf("UpperBody") == 0, "no brackets: none");
	Check(WornItemIdOf("(0001C6D1)") == 0, "no slot before it: none");
	Check(WornItemIdOf("UpperBody(0001C6D1)") == 0, "no space before the bracket: none");
	Check(WornItemIdOf("UpperBody (0001C6D)") == 0, "seven digits: none");
	Check(WornItemIdOf("UpperBody (0001C6DX)") == 0, "not hex: none");
	Check(WornItemIdOf("UpperBody (0001C6D1) x") == 0, "more after the bracket: none");
	Check(WornItemIdOf(nullptr) == 0, "no name: none");
}

void TestPoints() {
	std::printf("the armour's points\n");
	Check(ArmorPointsOf(0) == 0 && ArmorPointsOf(500) == 5 && ArmorPointsOf(1550) == 15, "the rating over a hundred");
	Check(ArmorPointsOf(65535) == 255, "held to a byte");
	UInt32 counts[kBodyMaterialCount] = {0, 3, 0, 9, 1};
	Check(MostCommonMaterial(counts) == BodyMaterial::Light, "most vertices light: light");
	UInt32 none[kBodyMaterialCount] = {0, 0, 0, 0, 0};
	Check(MostCommonMaterial(none) == BodyMaterial::Unknown, "no vertex: unknown");
	UInt32 unknownMost[kBodyMaterialCount] = {7, 2, 0, 0, 0};
	Check(MostCommonMaterial(unknownMost) == BodyMaterial::Unknown, "most of them unknown: unknown");
}

const NiPoint3 kA{0.0f, 0.0f, 0.0f};
const NiPoint3 kB{0.0f, 0.0f, 40.0f};

// A body of radius 6 along kA-kB, its skin all round; a breastplate of
// radius 9 over its front half (x > 0) from 10 to 30 up, light, 5 points.
void Body(std::vector<NiPoint3>& v, std::vector<UInt8>& material, std::vector<UInt8>& rating) {
	for (float z = -4.0f; z <= 44.0f; z += 0.5f) {
		for (int k = 0; k < 120; ++k) {
			const float t = 2.0f * math::kPi * static_cast<float>(k) / 120.0f;
			v.push_back(NiPoint3{6.0f * math::Cos(t), 6.0f * math::Sin(t), z});
			material.push_back(static_cast<UInt8>(BodyMaterial::Skin));
			rating.push_back(0);
			const float x = 9.0f * math::Cos(t);
			if (x > 0.0f && z >= 10.0f && z <= 30.0f) {
				v.push_back(NiPoint3{x, 9.0f * math::Sin(t), z});
				material.push_back(static_cast<UInt8>(BodyMaterial::Light));
				rating.push_back(5);
			}
		}
	}
}

void TestMeshGapVertex() {
	std::printf("the mesh gap names the vertex that is the surface\n");
	std::vector<NiPoint3> v;
	std::vector<UInt8> m;
	std::vector<UInt8> r;
	Body(v, m, r);
	const float lanes[1] = {1.0f};
	const MeshGap front = MeasureMeshGapIn(NiPoint3{12.0f, 0.0f, 20.0f}, kA, kB, v.data(), static_cast<UInt32>(v.size()),
	                                       lanes, 1, true);
	Check(front.measured && front.surfaceVertex < v.size() &&
	          m[front.surfaceVertex] == static_cast<UInt8>(BodyMaterial::Light),
	      "in front, the outermost layer: a vertex of the breastplate");
	const MeshGap back = MeasureMeshGapIn(NiPoint3{-12.0f, 0.0f, 20.0f}, kA, kB, v.data(), static_cast<UInt32>(v.size()),
	                                      lanes, 1, true);
	Check(back.measured && back.surfaceVertex < v.size() && m[back.surfaceVertex] == static_cast<UInt8>(BodyMaterial::Skin),
	      "behind: a vertex of the skin");
	const MeshGap none = MeasureMeshGapIn(NiPoint3{12.0f, 0.0f, 20.0f}, kA, kB, v.data(), 0, lanes, 1, true);
	Check(!none.measured && none.surfaceVertex == kNoSurfaceVertex, "no vertices: no vertex named");
}

void TestField() {
	std::printf("a field's bins carry the material and read it back\n");
	std::vector<NiPoint3> v;
	std::vector<UInt8> m;
	std::vector<UInt8> r;
	Body(v, m, r);
	const RadialField f = BuildRadialField(kA, kB, 8.0f, v.data(), static_cast<UInt32>(v.size()), nullptr, m.data(), r.data());
	const FieldPlace home = HomePlace(f, kA, kB);
	UInt8 points = 99;
	Check(f.classed, "built with materials: classed");
	Check(FieldMaterialAt(f, home, NiPoint3{12.0f, 0.0f, 20.0f}, &points) == BodyMaterial::Light && points == 5,
	      "in front at the chest: light armour, 5 points");
	Check(FieldMaterialAt(f, home, NiPoint3{-12.0f, 0.0f, 20.0f}, &points) == BodyMaterial::Skin && points == 0,
	      "behind: skin, no points");
	Check(FieldMaterialAt(f, home, NiPoint3{12.0f, 0.0f, 2.0f}) == BodyMaterial::Skin, "in front below the plate: skin");
	Check(FieldMaterialAt(f, home, NiPoint3{0.0f, 0.0f, 60.0f}) == BodyMaterial::Skin, "past the far end: skin");
	const RadialField plain = BuildRadialField(kA, kB, 8.0f, v.data(), static_cast<UInt32>(v.size()));
	Check(!plain.classed && FieldMaterialAt(plain, HomePlace(plain, kA, kB), NiPoint3{12.0f, 0.0f, 20.0f}, &points) ==
	                            BodyMaterial::Unknown && points == 0,
	      "built without materials: unknown");
	const RadialField empty = BuildRadialField(kA, kB, 8.0f, v.data(), 0, nullptr, m.data(), r.data());
	Check(empty.classed && FieldMaterialAt(empty, HomePlace(empty, kA, kB), NiPoint3{12.0f, 0.0f, 20.0f}) ==
	                           BodyMaterial::Unknown,
	      "no vertex at all: unknown in every bin");
}

void TestBodyCapsule() {
	std::printf("a body capsule's material where a point is\n");
	std::vector<NiPoint3> v;
	std::vector<UInt8> m;
	std::vector<UInt8> r;
	Body(v, m, r);
	const RadialField f = BuildRadialField(kA, kB, 8.0f, v.data(), static_cast<UInt32>(v.size()), nullptr, m.data(), r.data());
	BladeBodyCapsule c;
	c.a = kA;
	c.b = kB;
	c.radius = 8.0f;
	FieldRef ref;
	ref.field = &f;
	ref.u = f.u;
	ref.w = f.w;
	c.shape = ref;
	UInt8 points = 0;
	Check(BodyMaterialAt(c, NiPoint3{12.0f, 0.0f, 20.0f}, &points) == BodyMaterial::Light && points == 5,
	      "a fitted capsule: its field's material");
	BladeBodyCapsule guessed;
	guessed.a = kA;
	guessed.b = kB;
	guessed.radius = 8.0f;
	Check(BodyMaterialAt(guessed, NiPoint3{12.0f, 0.0f, 20.0f}, &points) == BodyMaterial::Unknown && points == 0,
	      "a guessed capsule, no field: unknown");
}

void TestFittedFields() {
	std::printf("the fitted fields carry each vertex's material\n");
	std::vector<NiPoint3> v;
	std::vector<UInt8> m;
	std::vector<UInt8> r;
	Body(v, m, r);
	std::vector<FitVertex> fv;
	for (UInt32 i = 0; i < v.size(); ++i) {
		FitVertex x{v[i], 0};
		x.material = m[i];
		x.rating = r[i];
		fv.push_back(x);
	}
	FitBone bone{"Bip01 Spine2", obvr::NiTransform{}};
	bone.skinToBone.rot = obvr::NiMatrix33::Identity();
	bone.skinToBone.pos = NiPoint3{0.0f, 0.0f, 0.0f};
	bone.skinToBone.scale = 1.0f;
	FittedCapsule c;
	c.group = FitGroupOfBone(bone.name);
	c.frameBone = 0;
	c.a = kA;
	c.b = kB;
	c.radius = 8.0f;
	const std::vector<RadialField> fields = BuildFittedFields(&bone, 1, fv.data(), static_cast<UInt32>(fv.size()), &c, 1);
	UInt8 points = 0;
	Check(fields.size() == 1 && fields[0].classed &&
	          FieldMaterialAt(fields[0], HomePlace(fields[0], kA, kB), NiPoint3{12.0f, 0.0f, 20.0f}, &points) ==
	              BodyMaterial::Light &&
	          points == 5,
	      "the breastplate's bins light, 5 points");
}

}  // namespace

int main() {
	TestWorn();
	TestNames();
	TestPoints();
	TestMeshGapVertex();
	TestField();
	TestBodyCapsule();
	TestFittedFields();
	if (g_failures != 0) {
		std::printf("%d FAILED\n", g_failures);
		return 1;
	}
	std::printf("all passed\n");
	return 0;
}
