// Checks OBVR's stand-in bones (game/FakeBone.h): every one has a parent the
// engine can read - 0x007E4960 reads [bone+0x1C]'s world position without a
// check (the Quest 2 tester's crash at Oblivion.exe+3E4A11, 2026-11-19) - the
// parent at the bone's place, and the parent itself the end of the chain.

#include <cstdio>
#include <cstring>

#include "game/FakeBone.h"

using namespace obvr;
using namespace obvr::game;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

NiTransform At(float x, float y, float z, float scale) {
	NiTransform t{};
	t.rot.data[0][0] = 1.0f;
	t.rot.data[1][1] = 1.0f;
	t.rot.data[2][2] = 1.0f;
	t.pos = NiPoint3{x, y, z};
	t.scale = scale;
	return t;
}

bool SameTransform(const NiTransform& a, const NiTransform& b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }

// A real bone as the engine has it: a vtable, a name, a parent, a controller
// and extra data - what a stand-in must not carry over.
struct RealBone {
	NiAVObject node{};
	NiAVObject parent{};
	int vtable = 0;
	int controller = 0;
	void* extra[2] = {};
};

void MakeReal(RealBone& r) {
	r.node.vtable = &r.vtable;
	r.node.refCount = 3;
	r.node.name = "Bip01 L Forearm";
	r.node.controller = &r.controller;
	r.node.extraDataList = r.extra;
	r.node.extraDataListLen = 2;
	r.node.extraDataListCapacity = 2;
	r.node.flags = 0x0008;
	r.node.parent = &r.parent;
	r.node.worldTransform = At(10.0f, 20.0f, 30.0f, 1.0f);
}

void TestMade() {
	std::printf("A stand-in bone made from a real one\n");
	RealBone real;
	MakeReal(real);
	static FakeBone fake;
	std::memset(&fake, 0xCD, sizeof(fake));
	MakeFakeBone(fake, &real.node);
	NiAVObject* const bone = FakeBoneNode(fake);
	NiAVObject* const parent = FakeBoneParent(fake);
	Check(bone->parent == parent, "the bone has a parent: its own stand-in, never null (0x007E4960 reads it)");
	Check(bone->parent != &real.parent, "not the real bone's parent, which a load may free");
	Check(parent->parent == nullptr, "the parent ends the chain, as a tree's root does - no loop upwards");
	Check(bone->vtable == &real.vtable && parent->vtable == &real.vtable && bone->name == real.node.name,
	      "both keep the real bone's vtable and name, for anything that looks");
	Check(bone->refCount == 0x40000000u && parent->refCount == 0x40000000u,
	      "both with a reference count nothing brings to zero");
	Check(bone->controller == nullptr && parent->controller == nullptr && bone->extraDataList == nullptr &&
	          parent->extraDataList == nullptr && bone->extraDataListLen == 0 && parent->extraDataListCapacity == 0,
	      "neither carries the real bone's controller or extra data");
	Check(SameTransform(parent->worldTransform, real.node.worldTransform) &&
	          SameTransform(bone->worldTransform, real.node.worldTransform),
	      "made where the real bone is, the parent with it");
	bool tailZero = true;
	for (UInt32 i = sizeof(NiAVObject); i < sizeof(fake.bytes); ++i) {
		tailZero = tailZero && fake.bytes[i] == 0 && fake.parentBytes[i] == 0;
	}
	Check(tailZero, "past the NiAVObject both are zero (a node's children: none)");
	Check(real.node.parent == &real.parent && real.node.refCount == 3, "the real bone is left as it was");
}

void TestPlaced() {
	std::printf("A stand-in bone moved\n");
	RealBone real;
	MakeReal(real);
	static FakeBone fake;
	MakeFakeBone(fake, &real.node);
	const NiTransform elbow = At(-5.0f, 7.0f, 120.0f, 0.001f);
	PlaceFakeBone(FakeBoneNode(fake), elbow);
	Check(SameTransform(FakeBoneNode(fake)->worldTransform, elbow), "the bone where it was put");
	Check(SameTransform(FakeBoneParent(fake)->worldTransform, elbow),
	      "its parent with it: what the engine reads as the parent's place is the bone's");

	// Written field by field (Dismember's Place), then synced.
	FakeBoneNode(fake)->worldTransform.pos = NiPoint3{1.0f, 2.0f, 3.0f};
	Check(!SameTransform(FakeBoneParent(fake)->worldTransform, FakeBoneNode(fake)->worldTransform),
	      "a write of the bone alone leaves the parent behind");
	SyncFakeBoneParent(fake);
	Check(SameTransform(FakeBoneParent(fake)->worldTransform, FakeBoneNode(fake)->worldTransform),
	      "the sync brings it along");

	// A node without a stand-in parent: placed, nothing else written.
	NiAVObject lone{};
	PlaceFakeBone(&lone, elbow);
	Check(SameTransform(lone.worldTransform, elbow) && lone.parent == nullptr,
	      "a bone with no parent is placed and nothing else touched");
}

}  // namespace

int main() {
	TestMade();
	TestPlaced();
	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
