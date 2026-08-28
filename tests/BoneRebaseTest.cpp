// Checks the decision half of the bone lock: which second-render bone rows
// are correct for their eye and which are instance mixups, and what a mixup
// is served instead. The stakes are stereo itself - a Keep verdict on a
// mixup collapses a body, a Mixup verdict on a correct row takes the
// second eye's parallax away.

#include <cstdio>
#include <cstring>

#include "render/BoneRebase.h"

namespace {

using obvr::render::AddEyeDeltaSample;
using obvr::render::BeginEyeDeltaFrame;
using obvr::render::BoneRowVerdict;
using obvr::render::BoneTranslationDistSq;
using obvr::render::CurrentEyeDelta;
using obvr::render::EyeDeltaEstimate;
using obvr::render::JudgeBoneRow;
using obvr::render::kBoneMixupThresholdSq;
using obvr::render::kBoneRowFloats;
using obvr::render::RebaseBoneRow;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

// A recognisable bone row: rotation floats carry their index, translation
// (floats 3, 7, 11) is the given point.
void MakeRow(float* row, float tx, float ty, float tz) {
	for (UInt32 i = 0; i < kBoneRowFloats; ++i) {
		row[i] = static_cast<float>(i) * 0.125f;
	}
	row[3] = tx;
	row[7] = ty;
	row[11] = tz;
}

void TestTranslationDistance() {
	std::printf("Translation distance\n");

	float a[kBoneRowFloats];
	float b[kBoneRowFloats];
	MakeRow(a, 10.0f, 20.0f, 30.0f);
	MakeRow(b, 13.0f, 24.0f, 30.0f);
	Check(BoneTranslationDistSq(a, a) == 0.0f, "a row is at distance zero from itself");
	Check(BoneTranslationDistSq(a, b) == 25.0f, "3-4-0 apart measures 25 squared");

	// Rotation floats must not contribute: perturb one and remeasure.
	b[5] += 100.0f;
	Check(BoneTranslationDistSq(a, b) == 25.0f, "rotation floats do not count");
}

void TestVerdict() {
	std::printf("Verdict\n");

	Check(JudgeBoneRow(0.0f, kBoneMixupThresholdSq) == BoneRowVerdict::Correct,
	      "identical translations are correct");
	// The eye baseline measured in the log: (-4.22, -1.87, -0.16).
	const float eyeSq = 4.22f * 4.22f + 1.87f * 1.87f + 0.16f * 0.16f;
	Check(JudgeBoneRow(eyeSq, kBoneMixupThresholdSq) == BoneRowVerdict::Correct,
	      "the measured eye baseline is correct");
	Check(JudgeBoneRow(kBoneMixupThresholdSq, kBoneMixupThresholdSq) ==
	          BoneRowVerdict::Correct,
	      "the threshold itself still passes");
	// The instance mixup measured in the probes: ~85 units apart.
	Check(JudgeBoneRow(85.0f * 85.0f, kBoneMixupThresholdSq) == BoneRowVerdict::Mixup,
	      "a body length apart is a mixup");
	Check(JudgeBoneRow(kBoneMixupThresholdSq + 1.0f, kBoneMixupThresholdSq) ==
	          BoneRowVerdict::Mixup,
	      "just past the threshold is a mixup");
}

void TestDeltaBeforeAnySample() {
	std::printf("Delta before any sample\n");

	EyeDeltaEstimate e;
	float delta[3] = {9.0f, 9.0f, 9.0f};
	CurrentEyeDelta(e, delta);
	Check(delta[0] == 0.0f && delta[1] == 0.0f && delta[2] == 0.0f,
	      "no samples ever means delta zero - the old blanket lock");

	BeginEyeDeltaFrame(e);
	CurrentEyeDelta(e, delta);
	Check(delta[0] == 0.0f && delta[1] == 0.0f && delta[2] == 0.0f,
	      "an empty frame does not invent a fallback");
}

void TestDeltaWithinFrame() {
	std::printf("Delta within a frame\n");

	EyeDeltaEstimate e;
	BeginEyeDeltaFrame(e);

	float logged[kBoneRowFloats];
	float incoming[kBoneRowFloats];
	MakeRow(logged, 100.0f, 200.0f, 300.0f);
	MakeRow(incoming, 96.0f, 198.0f, 300.0f);
	AddEyeDeltaSample(e, incoming, logged);

	float delta[3];
	CurrentEyeDelta(e, delta);
	Check(delta[0] == -4.0f && delta[1] == -2.0f && delta[2] == 0.0f,
	      "one sample is the delta verbatim");

	MakeRow(logged, 0.0f, 0.0f, 0.0f);
	MakeRow(incoming, -6.0f, -4.0f, 0.0f);
	AddEyeDeltaSample(e, incoming, logged);
	CurrentEyeDelta(e, delta);
	Check(delta[0] == -5.0f && delta[1] == -3.0f && delta[2] == 0.0f,
	      "two samples average");
	Check(e.samples == 2, "and both are counted");
}

void TestDeltaAcrossFrames() {
	std::printf("Delta across frames\n");

	EyeDeltaEstimate e;
	BeginEyeDeltaFrame(e);

	float logged[kBoneRowFloats];
	float incoming[kBoneRowFloats];
	MakeRow(logged, 0.0f, 0.0f, 0.0f);
	MakeRow(incoming, -4.0f, -2.0f, -0.5f);
	AddEyeDeltaSample(e, incoming, logged);

	BeginEyeDeltaFrame(e);
	Check(e.samples == 0, "a new frame starts empty");
	float delta[3];
	CurrentEyeDelta(e, delta);
	Check(delta[0] == -4.0f && delta[1] == -2.0f && delta[2] == -0.5f,
	      "but falls back to the finished frame's mean");

	// A sample in the new frame takes over from the fallback immediately.
	MakeRow(incoming, 8.0f, 6.0f, 1.0f);
	AddEyeDeltaSample(e, incoming, logged);
	CurrentEyeDelta(e, delta);
	Check(delta[0] == 8.0f && delta[1] == 6.0f && delta[2] == 1.0f,
	      "a fresh sample outranks the previous frame");

	// A frame that measured nothing keeps the older fallback alive.
	BeginEyeDeltaFrame(e);
	BeginEyeDeltaFrame(e);
	CurrentEyeDelta(e, delta);
	Check(delta[0] == 8.0f && delta[1] == 6.0f && delta[2] == 1.0f,
	      "an empty frame does not erase the fallback");
}

void TestRebase() {
	std::printf("Rebase\n");

	float logged[kBoneRowFloats];
	MakeRow(logged, 100.0f, 200.0f, 300.0f);
	const float delta[3] = {-4.0f, -2.0f, -0.5f};

	float out[kBoneRowFloats];
	RebaseBoneRow(out, logged, delta);
	Check(out[3] == 96.0f && out[7] == 198.0f && out[11] == 299.5f,
	      "the translation moves by the delta");

	// Every rotation float must be a bit-for-bit copy: the replace pass
	// pairs by rotation fingerprint, and a drifted copy would never pair.
	bool rotationIntact = true;
	for (UInt32 i = 0; i < kBoneRowFloats; ++i) {
		if (i == 3 || i == 7 || i == 11) {
			continue;
		}
		if (std::memcmp(&out[i], &logged[i], sizeof(float)) != 0) {
			rotationIntact = false;
		}
	}
	Check(rotationIntact, "the rotation floats are untouched");
}

}  // namespace

int main() {
	TestTranslationDistance();
	TestVerdict();
	TestDeltaBeforeAnySample();
	TestDeltaWithinFrame();
	TestDeltaAcrossFrames();
	TestRebase();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
