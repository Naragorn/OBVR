// Checks the geometry half of the bone lock: which second-render rows count
// as baseline measurements, how the palette convention's sign is calibrated
// against the camera shift, which delta a replaced row is shifted by, and
// that the shift never touches the rotation fingerprint. The stakes are
// stereo itself - a wrong delta moves every skinned body off its eye.

#include <cstdio>
#include <cstring>

#include "render/BoneRebase.h"

namespace {

using obvr::render::AddEyeDeltaSample;
using obvr::render::BeginEyeDeltaFrame;
using obvr::render::BoneTranslationDistSq;
using obvr::render::CalibrateShiftSign;
using obvr::render::ChooseRebaseDelta;
using obvr::render::CurrentEyeDelta;
using obvr::render::EyeDeltaEstimate;
using obvr::render::IsEyeBaselineSample;
using obvr::render::kBoneMixupThresholdSq;
using obvr::render::kBoneRowFloats;
using obvr::render::kBoneStaleThresholdSq;
using obvr::render::RebaseBoneRow;
using obvr::render::ShiftSign;

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

// Feeds the estimator one sample of exactly (dx, dy, dz).
void FeedSample(EyeDeltaEstimate& e, float dx, float dy, float dz) {
	float logged[kBoneRowFloats];
	float incoming[kBoneRowFloats];
	MakeRow(logged, 0.0f, 0.0f, 0.0f);
	MakeRow(incoming, dx, dy, dz);
	AddEyeDeltaSample(e, incoming, logged);
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

void TestBaselineBand() {
	std::printf("Baseline band\n");

	Check(!IsEyeBaselineSample(0.0f), "bit-identical rows measure nothing");
	Check(!IsEyeBaselineSample(kBoneStaleThresholdSq),
	      "the stale threshold itself is still stale");
	// The eye baseline measured in the log: (-4.22, -1.87, -0.16).
	const float eyeSq = 4.22f * 4.22f + 1.87f * 1.87f + 0.16f * 0.16f;
	Check(IsEyeBaselineSample(eyeSq), "the measured eye baseline is a sample");
	Check(IsEyeBaselineSample(kBoneMixupThresholdSq),
	      "the mixup threshold itself still measures");
	// The instance mixup measured in the probes: ~85 units apart.
	Check(!IsEyeBaselineSample(85.0f * 85.0f), "a body length apart is a mixup");
	Check(!IsEyeBaselineSample(kBoneMixupThresholdSq + 1.0f),
	      "just past the mixup threshold measures nothing");
}

void TestMeasurementStates() {
	std::printf("Measurement states\n");

	EyeDeltaEstimate e;
	float delta[3] = {9.0f, 9.0f, 9.0f};
	CurrentEyeDelta(e, delta);
	Check(delta[0] == 0.0f && delta[1] == 0.0f && delta[2] == 0.0f,
	      "no samples ever means delta zero - the old blanket lock");

	BeginEyeDeltaFrame(e);
	FeedSample(e, -4.0f, -2.0f, 0.0f);
	CurrentEyeDelta(e, delta);
	Check(delta[0] == -4.0f && delta[1] == -2.0f && delta[2] == 0.0f,
	      "one sample is the delta verbatim");

	FeedSample(e, -6.0f, -4.0f, 0.0f);
	CurrentEyeDelta(e, delta);
	Check(delta[0] == -5.0f && delta[1] == -3.0f && delta[2] == 0.0f,
	      "two samples average");

	BeginEyeDeltaFrame(e);
	Check(e.samples == 0, "a new frame starts empty");
	CurrentEyeDelta(e, delta);
	Check(delta[0] == -5.0f && delta[1] == -3.0f && delta[2] == 0.0f,
	      "but falls back to the finished frame's mean");

	FeedSample(e, 8.0f, 6.0f, 1.0f);
	CurrentEyeDelta(e, delta);
	Check(delta[0] == 8.0f && delta[1] == 6.0f && delta[2] == 1.0f,
	      "a fresh sample outranks the previous frame");

	BeginEyeDeltaFrame(e);
	BeginEyeDeltaFrame(e);
	CurrentEyeDelta(e, delta);
	Check(delta[0] == 8.0f && delta[1] == 6.0f && delta[2] == 1.0f,
	      "an empty frame does not erase the fallback");
}

void TestSignCalibration() {
	std::printf("Sign calibration\n");

	const float shift[3] = {-1.1f, 4.5f, 0.0f};

	EyeDeltaEstimate e;
	BeginEyeDeltaFrame(e);
	for (int i = 0; i < 8; ++i) {
		FeedSample(e, -1.1f, 4.5f, 0.0f);
	}
	Check(CalibrateShiftSign(e, shift, 8) == ShiftSign::Positive,
	      "a measurement along the shift calibrates positive");

	EyeDeltaEstimate n;
	BeginEyeDeltaFrame(n);
	for (int i = 0; i < 8; ++i) {
		FeedSample(n, 1.1f, -4.5f, 0.0f);
	}
	Check(CalibrateShiftSign(n, shift, 8) == ShiftSign::Negative,
	      "a measurement against the shift calibrates negative");

	EyeDeltaEstimate few;
	BeginEyeDeltaFrame(few);
	for (int i = 0; i < 7; ++i) {
		FeedSample(few, -1.1f, 4.5f, 0.0f);
	}
	Check(CalibrateShiftSign(few, shift, 8) == ShiftSign::Unknown,
	      "too few samples stay Unknown");

	const float noShift[3] = {0.0f, 0.0f, 0.0f};
	Check(CalibrateShiftSign(e, noShift, 8) == ShiftSign::Unknown,
	      "a camera that did not move calibrates nothing");

	EyeDeltaEstimate ortho;
	BeginEyeDeltaFrame(ortho);
	for (int i = 0; i < 8; ++i) {
		FeedSample(ortho, 4.5f, 1.1f, 0.0f);
	}
	Check(CalibrateShiftSign(ortho, shift, 8) == ShiftSign::Unknown,
	      "a measurement orthogonal to the shift refuses to commit");
}

void TestChooseRebaseDelta() {
	std::printf("Choosing the rebase delta\n");

	const float shift[3] = {-1.1f, 4.5f, 0.0f};
	EyeDeltaEstimate e;
	BeginEyeDeltaFrame(e);
	FeedSample(e, 2.0f, 3.0f, 0.5f);

	float delta[3];
	ChooseRebaseDelta(e, shift, ShiftSign::Unknown, delta);
	Check(delta[0] == 2.0f && delta[1] == 3.0f && delta[2] == 0.5f,
	      "uncalibrated falls back to the measurement");

	ChooseRebaseDelta(e, shift, ShiftSign::Positive, delta);
	Check(delta[0] == -1.1f && delta[1] == 4.5f && delta[2] == 0.0f,
	      "positive takes the camera shift as is");

	ChooseRebaseDelta(e, shift, ShiftSign::Negative, delta);
	Check(delta[0] == 1.1f && delta[1] == -4.5f && delta[2] == 0.0f,
	      "negative takes the camera shift negated");

	const float noShift[3] = {0.0f, 0.0f, 0.0f};
	ChooseRebaseDelta(e, noShift, ShiftSign::Positive, delta);
	Check(delta[0] == 0.0f && delta[1] == 0.0f && delta[2] == 0.0f,
	      "a camera that did not move shifts nothing even when calibrated");
}

void TestWorldSizedTarget() {
	std::printf("World-sized target\n");

	using obvr::render::BoneTargetIsWorldSized;
	Check(BoneTargetIsWorldSized(512, 0),
	      "an unmeasured main width claims every target for the world");
	Check(BoneTargetIsWorldSized(2560, 2560), "the world target is world-sized");
	Check(BoneTargetIsWorldSized(4096, 2560), "a larger target still counts");
	Check(!BoneTargetIsWorldSized(512, 2560),
	      "a shadow-sized target is a sub-pass");
	Check(!BoneTargetIsWorldSized(2559, 2560),
	      "just under the main width is a sub-pass");
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
	TestBaselineBand();
	TestMeasurementStates();
	TestSignCalibration();
	TestChooseRebaseDelta();
	TestWorldSizedTarget();
	TestRebase();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
