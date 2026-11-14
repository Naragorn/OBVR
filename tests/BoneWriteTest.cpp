#include <cstdio>
#include <cstring>

#include "game/BoneWrite.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %-5s %s\n", condition ? "ok" : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

using namespace obvr;
using namespace obvr::game;

NiTransform At(float x) {
	NiTransform t{};
	t.rot.data[0][0] = t.rot.data[1][1] = t.rot.data[2][2] = 1.0f;
	t.pos = NiPoint3{x, 0.0f, 0.0f};
	t.scale = 1.0f;
	return t;
}

void TestGiveBack() {
	std::printf("a write given back after the draw, so the next animation starts from the game's pose\n");
	alignas(16) static unsigned char storage[sizeof(NiAVObject)] = {};
	auto* node = reinterpret_cast<NiAVObject*>(storage);
	node->localTransform = At(-14.6f);  // the game's left hip
	BoneWrite w;
	UInt32 kept = 0;
	UInt32 renewed = 0;
	BeginWrite(w, node, kept, renewed);
	WriteLocal(w, At(14.6f));  // mirrored to the right
	Check(node->localTransform.pos.x == 14.6f, "the write in the node through the draw");
	GiveBackAfterDraw(w);
	Check(node->localTransform.pos.x == -14.6f, "after the draw: the game's pose back");
	Check(w.held, "the booking kept for the next step");
	BeginWrite(w, node, kept, renewed);
	Check(w.base.pos.x == -14.6f && renewed == 1 && kept == 0, "the next step reads the game's pose as its base");

	WriteLocal(w, At(14.6f));
	node->localTransform = At(-10.0f);  // the engine wrote it after us
	GiveBackAfterDraw(w);
	Check(node->localTransform.pos.x == -10.0f, "a bone the engine wrote again: left as the engine has it");

	BoneWrite none;
	GiveBackAfterDraw(none);
	Check(none.node == nullptr && !none.held, "nothing booked: nothing done");
	BoneWrite notHeld;
	notHeld.node = node;
	notHeld.wrote = node->localTransform;
	notHeld.base = At(3.0f);
	GiveBackAfterDraw(notHeld);
	Check(node->localTransform.pos.x == -10.0f, "not written this frame: left");
}

}  // namespace

int main() {
	TestGiveBack();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
