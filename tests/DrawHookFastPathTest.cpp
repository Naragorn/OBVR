// Checks the small decisions the device hooks take on every draw and every
// constant upload (render/DrawHookFastPath.h). They exist so that a draw pays
// for a question once: whether stream 0 is a buffer of the dynamic pool is
// asked of the set once per binding, and the helpers that only work in the
// menu's pass or on a bone row are not called outside them. Each flow here is one way a hook can
// go; a wrong one either costs the frame its time again or - worse - answers
// from a stale state.

#include <cstdio>

#include "render/DrawHookFastPath.h"

namespace {

using obvr::render::AfterPassSampleDue;
using obvr::render::BoneRowsWatched;
using obvr::render::DrawHookMemo;
using obvr::render::PointerSet;
using obvr::render::Stream0Membership;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

void* Fake(UInt32 n) {
	return reinterpret_cast<void*>(static_cast<UInt32>(0x1000 + n * 0x10));
}

// A set that counts how often it is searched: the cost the membership saves.
struct CountingSet {
	PointerSet set;
	UInt32 searches = 0;
	bool operator()(const void* pointer) {
		++searches;
		return set.Contains(pointer);
	}
};

void TestNothingBound() {
	std::printf("stream 0: nothing bound\n");
	Stream0Membership stream;
	CountingSet pool;
	pool.set.Insert(Fake(1));
	Check(!stream.DynamicBy(pool), "no buffer is not a dynamic one");
	Check(pool.searches == 0, "and the set is not searched for it");
}

void TestAskedOncePerBinding() {
	std::printf("stream 0: one search per binding\n");
	Stream0Membership stream;
	CountingSet pool;
	pool.set.Insert(Fake(1));
	stream.Bind(Fake(1));
	Check(stream.DynamicBy(pool), "a pool buffer bound reads as dynamic");
	Check(stream.DynamicBy(pool) && stream.DynamicBy(pool), "and again, draw after draw");
	Check(pool.searches == 1, "with one search for all of them");

	stream.Bind(Fake(2));
	Check(!stream.DynamicBy(pool), "a static buffer bound reads as not dynamic");
	Check(!stream.DynamicBy(pool), "and again");
	Check(pool.searches == 2, "with one more search, not two");
}

void TestSameBufferBoundAgain() {
	std::printf("stream 0: the same buffer bound again keeps its answer\n");
	Stream0Membership stream;
	CountingSet pool;
	pool.set.Insert(Fake(1));
	stream.Bind(Fake(1));
	Check(stream.DynamicBy(pool), "dynamic");
	// The pool's buffer is bound again for every skinned shape, at another
	// offset - the buffer is the same, so is the answer.
	stream.Bind(Fake(1));
	Check(stream.DynamicBy(pool), "still dynamic after the rebind");
	Check(pool.searches == 1, "without a second search");
}

void TestUnbound() {
	std::printf("stream 0: unbound\n");
	Stream0Membership stream;
	CountingSet pool;
	pool.set.Insert(Fake(1));
	stream.Bind(Fake(1));
	Check(stream.DynamicBy(pool), "dynamic while bound");
	stream.Bind(nullptr);
	Check(!stream.DynamicBy(pool), "nothing bound is not dynamic");
	stream.Bind(Fake(1));
	Check(stream.DynamicBy(pool), "bound again it is asked again and found");
	Check(pool.searches == 2, "one search per binding of a buffer");
}

void TestBecomesDynamicWhileBound() {
	std::printf("stream 0: the bound buffer joins the pool later\n");
	Stream0Membership stream;
	CountingSet pool;
	stream.Bind(Fake(3));
	Check(!stream.DynamicBy(pool), "not in the pool yet");
	// Its first discard lock comes while it is bound: the "no" is stale.
	pool.set.Insert(Fake(3));
	stream.NoteInserted(Fake(3));
	Check(stream.DynamicBy(pool), "dynamic once it was locked with discard");
	Check(pool.searches == 2, "asked again exactly once");
}

void TestAnotherBufferJoins() {
	std::printf("stream 0: another buffer joins the pool\n");
	Stream0Membership stream;
	CountingSet pool;
	stream.Bind(Fake(3));
	Check(!stream.DynamicBy(pool), "the bound one is static");
	pool.set.Insert(Fake(4));
	stream.NoteInserted(Fake(4));
	Check(!stream.DynamicBy(pool), "another buffer's lock does not change that");
	Check(pool.searches == 1, "and costs no search");
}

void TestInsertOfAKnownDynamic() {
	std::printf("stream 0: the bound pool buffer locked again\n");
	Stream0Membership stream;
	CountingSet pool;
	pool.set.Insert(Fake(1));
	stream.Bind(Fake(1));
	Check(stream.DynamicBy(pool), "dynamic");
	stream.NoteInserted(Fake(1));
	Check(stream.DynamicBy(pool), "its next discard lock leaves it dynamic");
	Check(pool.searches == 1, "a yes never goes stale - the set only grows");
}

void TestFullSet() {
	std::printf("stream 0: a buffer the full set refused\n");
	Stream0Membership stream;
	CountingSet pool;
	for (UInt32 i = 0; i < PointerSet::kCapacity; ++i) {
		pool.set.Insert(Fake(i));
	}
	stream.Bind(Fake(900));
	Check(!pool.set.Insert(Fake(900)), "the full set refuses it");
	stream.NoteInserted(Fake(900));
	Check(!stream.DynamicBy(pool), "so it reads as the set has it: not dynamic");
}

void TestPoolOverload() {
	std::printf("stream 0: asked of the pool itself\n");
	DrawHookMemo memo;
	PointerSet pool;
	pool.Insert(Fake(1));
	Check(!memo.stream0.Dynamic(pool), "nothing bound");
	memo.stream0.Bind(Fake(1));
	Check(memo.stream0.Dynamic(pool), "a pool buffer bound reads as dynamic");
	memo.stream0.Bind(Fake(2));
	Check(!memo.stream0.Dynamic(pool), "a static one as not");
	pool.Insert(Fake(2));
	memo.stream0.NoteInserted(Fake(2));
	Check(memo.stream0.Dynamic(pool), "and as dynamic once the pool took it");
	Check(!memo.waterShaderSelected, "a fresh memo has no water shader selected");
}

void TestAfterPassSample() {
	std::printf("the draw sample after the menu's pass\n");
	Check(!AfterPassSampleDue(false, false, 0), "world draw, nothing owed: not called");
	Check(!AfterPassSampleDue(false, true, 0), "world draw, the tail armed for the pass only: not called");
	Check(AfterPassSampleDue(false, false, 1), "after the pass with samples owed: called");
	Check(!AfterPassSampleDue(true, false, 0), "in the pass, the tail not armed: not called");
	Check(!AfterPassSampleDue(true, false, 4), "in the pass the owed samples wait for its end");
	Check(AfterPassSampleDue(true, true, 0), "in the pass with the tail armed: called");
}

void TestBoneRowsWatched() {
	std::printf("bone rows\n");
	Check(!BoneRowsWatched(false, 0), "no lock running and no probe: the rows are not looked at");
	Check(BoneRowsWatched(true, 0), "a lock running: looked at");
	Check(BoneRowsWatched(false, 1), "the stack probe owed: looked at");
	Check(BoneRowsWatched(true, 3), "both: looked at");
}

}  // namespace

int main() {
	TestNothingBound();
	TestAskedOncePerBinding();
	TestSameBufferBoundAgain();
	TestUnbound();
	TestBecomesDynamicWhileBound();
	TestAnotherBufferJoins();
	TestInsertOfAKnownDynamic();
	TestFullSet();
	TestPoolOverload();
	TestAfterPassSample();
	TestBoneRowsWatched();
	std::printf(g_failures == 0 ? "all passed\n" : "%d FAILED\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
