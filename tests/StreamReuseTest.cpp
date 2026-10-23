// Checks lever 2's decisions (render/StreamReuseLogic.h): the discard
// generations, the first pass's notes, every reason a second-pass pack is or
// is not skipped, the check mode's hash comparison, the ring capacity and the
// counters' line.

#include <cstdio>
#include <cstring>

#include "render/StreamReuseLogic.h"

using namespace obvr::render;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf("  %s  %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) {
		++g_failures;
	}
}

int g_bufferStorage[64];
void* Buffer(int i) { return &g_bufferStorage[i]; }
int g_recordStorage[64];
void* Record(int i) { return &g_recordStorage[i]; }

void TestGenerations() {
	std::printf("Discard generations\n");
	BufferGenerations g;
	Check(g.Of(Buffer(1)) == 0 && g.Trusted(Buffer(1)), "an unseen buffer reads generation 0 and is trusted");
	Check(g.Bump(Buffer(1)) && g.Of(Buffer(1)) == 1, "the first DISCARD makes it 1");
	Check(g.Bump(Buffer(1)) && g.Of(Buffer(1)) == 2, "the second 2");
	Check(g.Bump(Buffer(2)) && g.Of(Buffer(2)) == 1 && g.Of(Buffer(1)) == 2, "another buffer counts on its own");
	for (int i = 3; i < 3 + static_cast<int>(BufferGenerations::kSlots) - 2; ++i) {
		g.Bump(Buffer(i));
	}
	Check(g.count == BufferGenerations::kSlots && !g.overflowed, "the table full, nothing lost yet");
	Check(!g.Bump(Buffer(60)) && g.overflowed, "one more buffer: refused, the overflow marked");
	Check(g.Trusted(Buffer(1)) && !g.Trusted(Buffer(60)) && !g.Trusted(Buffer(61)),
	      "after the overflow a known buffer is trusted, an unknown one is not");
	Check(g.Bump(Buffer(1)) && g.Of(Buffer(1)) == 3, "known buffers still count");
}

void TestNotes() {
	std::printf("The first pass's notes\n");
	PackNotes notes;
	Check(notes.Find(Record(1)) == nullptr, "nothing noted: not found");
	PackNote* n = notes.Put(Record(1), 10);
	Check(n != nullptr && n->record == Record(1) && n->frame == 10 && n->bufferCount == 0, "a fresh note");
	n->AddBuffer(Buffer(1), 0);
	n->AddBuffer(Buffer(2), 0);
	n->AddBuffer(Buffer(1), 0);
	Check(n->bufferCount == 2 && !n->overflow, "a buffer is noted once");
	PackNote* again = notes.Put(Record(1), 11);
	Check(again == n && again->frame == 11 && again->bufferCount == 0, "the same record gets its slot back, fresh");
	Check(notes.Find(Record(1)) == again, "and is found");
	for (int i = 0; i < 5; ++i) {
		n->AddBuffer(Buffer(10 + i), 0);
	}
	Check(n->bufferCount == PackNote::kBuffers && n->overflow, "a fifth buffer overflows the note");
	BufferGenerations g;
	g.Bump(Buffer(10));
	g.Bump(Buffer(10));
	n->SnapshotGenerations(g);
	Check(n->generations[0] == 2 && n->generations[1] == 0, "the snapshot reads the table");
	for (int i = 0; i < 7; ++i) {
		n->AddHash(static_cast<UInt64>(i));
	}
	Check(n->hashCount == PackNote::kHashes && n->hashOverflow, "a seventh hash overflows");

	// A probe window full of this frame's notes gives nothing; an old note is reused.
	PackNotes crowded;
	void* records[PackNotes::kProbes + 1];
	UInt32 made = 0;
	for (UInt32 i = 0; i < PackNotes::kProbes + 1; ++i) {
		records[i] = reinterpret_cast<void*>(0x10000000u + i * (PackNotes::kSlots * 16u));   // the same home
		if (crowded.Put(records[i], 5) != nullptr) ++made;
	}
	Check(made == PackNotes::kProbes, "a full probe window: the last record finds no slot");
	Check(crowded.Put(Record(2), 7) != nullptr, "two frames on, a stale note is taken over");
}

void TestDecide() {
	std::printf("The second pass's decision\n");
	BufferGenerations g;
	g.Bump(Buffer(1));   // generation 1
	PackNotes notes;
	PackNote* n = notes.Put(Record(1), 20);
	n->AddBuffer(Buffer(1), 0);
	n->AddBuffer(Buffer(2), 0);
	n->SnapshotGenerations(g);
	Check(DecideReuse(false, n, 20, g) == ReuseWhy::Skip, "volatile, noted this frame, no DISCARD since: skip");
	Check(DecideReuse(true, n, 20, g) == ReuseWhy::Forced, "a forced repack is honoured");
	Check(DecideReuse(false, nullptr, 20, g) == ReuseWhy::NotNoted, "no note: pack");
	Check(DecideReuse(false, n, 21, g) == ReuseWhy::OtherFrame, "a note from another frame: pack");
	g.Bump(Buffer(2));
	Check(DecideReuse(false, n, 20, g) == ReuseWhy::Generation, "a DISCARD on one of its buffers since: pack");
	PackNote* empty = notes.Put(Record(2), 20);
	Check(DecideReuse(false, empty, 20, g) == ReuseWhy::Overflow, "a note that locked nothing: pack");
	PackNote* over = notes.Put(Record(3), 20);
	for (int i = 0; i < 5; ++i) over->AddBuffer(Buffer(20 + i), 0);
	over->SnapshotGenerations(g);
	Check(DecideReuse(false, over, 20, g) == ReuseWhy::Overflow, "a note that overflowed: pack");
	BufferGenerations full;
	for (int i = 0; i < static_cast<int>(BufferGenerations::kSlots) + 1; ++i) full.Bump(Buffer(i));
	PackNote* unknown = notes.Put(Record(4), 20);
	unknown->AddBuffer(Buffer(60), 0);
	unknown->SnapshotGenerations(full);
	Check(DecideReuse(false, unknown, 20, full) == ReuseWhy::Overflow,
	      "a buffer the overflowed table does not know: pack");
}

void TestHashes() {
	std::printf("The check mode\n");
	PackNote a, b;
	a.AddHash(1); a.AddHash(2);
	b.AddHash(1); b.AddHash(2);
	Check(HashesMatch(a, b), "the same hashes in the same order match");
	b.hashes[1] = 3;
	Check(!HashesMatch(a, b), "one differing hash: no match");
	PackNote c;
	c.AddHash(1);
	Check(!HashesMatch(a, c), "a different count: no match");
	PackNote d = a;
	d.hashOverflow = true;
	Check(!HashesMatch(a, d), "an overflowed side cannot match");
	const UInt8 bytes1[] = {1, 2, 3, 4, 5};
	const UInt8 bytes2[] = {1, 2, 3, 4, 6};
	Check(HashBytes(bytes1, 5) == HashBytes(bytes1, 5) && HashBytes(bytes1, 5) != HashBytes(bytes2, 5) &&
	          HashBytes(bytes1, 4) != HashBytes(bytes1, 5),
	      "the hash tells bytes and lengths apart");
}

void TestClasses() {
	std::printf("The classes\n");
	ClassVerdicts v;
	v.Note(0xA0001000, false, 1275);
	v.Note(0xA0001000, true, 1275);
	v.Note(0xA0002000, false, 300);
	Check(v.count == 2 && v.compared[0] == 2 && v.mismatched[0] == 1 && v.compared[1] == 1 && v.mismatched[1] == 0,
	      "two classes counted apart");
	char line[512];
	v.Summary(line, sizeof(line));
	Check(std::strstr(line, "A0001000: 2 compared, 1 mismatched (e.g. 1275 vertices); A0002000: 1 compared, 0 mismatched") == line,
	      "the summary names both");
	for (UInt32 i = 0; i < ClassVerdicts::kClasses + 2; ++i) v.Note(0xB0000000 + i * 16, false, 1);
	Check(v.overflow, "more classes than slots is said");
	PackTypes types;
	types.Count(0x8003);
	types.Count(0x8000);
	types.Count(0x4000);
	char line2[128];
	types.Summary(line2, sizeof(line2));
	Check(std::strcmp(line2, "4000:1 8000:2") == 0, "the packs by type nibble");
	BufferGenerations g;
	g.Bump(Buffer(1));
	g.Bump(Buffer(1));
	g.Bump(Buffer(2));
	char line3[128];
	g.Summary(line3, sizeof(line3));
	Check(std::strstr(line3, ":2 ") != nullptr && std::strstr(line3 + 2, ":1") != nullptr, "the buffers' discard counts");
}

void TestSamplesAndTrust() {
	std::printf("The source samples and the trust\n");
	// 100 elements of 12 bytes in a 36-byte stride, the gaps full of garbage.
	UInt8 a[3600], b[3600];
	for (int i = 0; i < 3600; ++i) {
		a[i] = static_cast<UInt8>(i * 7);
		b[i] = (i % 36) < 12 ? a[i] : static_cast<UInt8>(0xEE);   // the same elements, other gaps
	}
	Check(ElementHash(a, 100, 12, 36, kSampleElements) == ElementHash(b, 100, 12, 36, kSampleElements),
	      "the same elements hash the same whatever the gaps hold");
	Check(ElementHash(a, 100, 12, 36, 0) == ElementHash(b, 100, 12, 36, 0), "the full hash too");
	b[36 * 99 + 5] ^= 1;   // the last element
	Check(ElementHash(a, 100, 12, 36, kSampleElements) != ElementHash(b, 100, 12, 36, kSampleElements),
	      "the last element is sampled");
	b[36 * 99 + 5] ^= 1;
	b[36 * 50 + 5] ^= 1;   // an element between the samples
	Check(ElementHash(a, 100, 12, 36, kSampleElements) == ElementHash(b, 100, 12, 36, kSampleElements),
	      "an element between the samples is not seen by the sample");
	Check(ElementHash(a, 100, 12, 36, 0) != ElementHash(b, 100, 12, 36, 0), "but by the full hash");
	b[36 * 50 + 5] ^= 1;
	Check(ElementHash(a, 100, 12, 36, 0) != ElementHash(a, 99, 12, 36, 0), "the count counts");
	Check(ElementHash(a, 10, 12, 36, kSampleElements) == ElementHash(a, 10, 12, 36, 0), "fewer elements than samples: all taken");

	PackNote first, second;
	first.AddSample(1); first.AddSample(2);
	second.AddSample(1); second.AddSample(2);
	Check(SamplesMatch(first, second), "the same samples match");
	second.samples[0] = 9;
	Check(!SamplesMatch(first, second), "a differing sample: no match");
	PackNote none;
	Check(!SamplesMatch(none, none), "no samples at all cannot match");

	TrustNotes trust;
	Check(trust.Find(Record(1)) == nullptr && DecideTrust(nullptr, 10) == TrustStep::Verify, "an unknown record is verified");
	RecordTrust* t = trust.Get(Record(1), 10);
	Check(t != nullptr && t->record == Record(1) && DecideTrust(t, 10) == TrustStep::Verify, "a new record is verified");
	t->matches = kLearnFrames - 1;
	Check(DecideTrust(t, 11) == TrustStep::Verify, "one match short of the learning: verified");
	t->matches = kLearnFrames;
	t->lastVerified = 11;
	Check(DecideTrust(t, 12) == TrustStep::Skip, "learned and fresh: skipped");
	Check(DecideTrust(t, 11 + kVerifyEvery) == TrustStep::Verify, "due for a re-check: verified");
	Check(DecideTrust(t, 11 + kVerifyEvery - 1) == TrustStep::Skip, "a frame before that: still skipped");
	t->mismatches = 1;
	Check(DecideTrust(t, 12) == TrustStep::Never, "one mismatch ever: never again");
	Check(trust.Get(Record(1), 20) == t && t->lastSeen == 20, "Get finds the record and notes the frame");
	Check(trust.Find(Record(1)) == t, "Find finds it");
	RecordTrust* other = trust.Get(Record(2), 20);
	Check(other != nullptr && other != t && other->matches == 0, "another record gets its own slot");
	other->lastSeen = 1;
	RecordTrust* reused = trust.Get(Record(3), 1 + TrustNotes::kStaleFrames);
	Check(reused != nullptr, "a long unseen record's slot can be given up");
}

void TestRing() {
	std::printf("The ring capacity\n");
	Check(RingCapacityWanted(112224, 20000, 4194304) == 4194304, "a small ring is raised to the minimum");
	Check(RingCapacityWanted(4194304, 20000, 4194304) == 0, "a ring at the minimum is left alone");
	Check(RingCapacityWanted(8000000, 20000, 4194304) == 0, "a bigger ring is left alone");
	Check(RingCapacityWanted(2048, 5000000, 4194304) == 0,
	      "a request above the minimum is left to the engine's own growth");
	Check(RingCapacityWanted(0, 2048, 4194304) == 4194304, "a ring not yet created gets the minimum");
}

void TestSummary() {
	std::printf("The counters' line\n");
	ReuseCounters c;
	c.firstPassPacks = 100;
	c.secondPassPacks = 100;
	for (int i = 0; i < 90; ++i) c.Count(ReuseWhy::Skip);
	for (int i = 0; i < 7; ++i) c.Count(ReuseWhy::Generation);
	for (int i = 0; i < 3; ++i) c.Count(ReuseWhy::NotNoted);
	c.ringsGrown = 6;
	char line[640];
	c.Summary(line, sizeof(line));
	Check(std::strstr(line, "second pass asked 100, skipped 90 (90%)") != nullptr, "the skip share");
	Check(std::strstr(line, "forced 0, not noted 3, other frame 0, generation 7") != nullptr, "the reasons");
	Check(std::strstr(line, "rings grown 6") != nullptr, "the rings");
	Check(std::strstr(line, "verifying 0, eye-dependent 0; verified 0 (matched 0, mismatched 0, empty 0)") != nullptr, "the trust");
	ReuseCounters none;
	none.Summary(line, sizeof(line));
	Check(std::strstr(line, "skipped 0 (0%)") != nullptr, "no packs: no division by zero");
}

}  // namespace

int main() {
	TestGenerations();
	TestNotes();
	TestDecide();
	TestHashes();
	TestClasses();
	TestSamplesAndTrust();
	TestRing();
	TestSummary();
	if (g_failures != 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
