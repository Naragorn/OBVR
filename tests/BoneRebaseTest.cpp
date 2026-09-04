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
using obvr::render::BoneMatch;
using obvr::render::BoneMatchKind;
using obvr::render::BoneRow;
using obvr::render::BoneTranslationDistSq;
using obvr::render::FindBoneRow;
using obvr::render::SameBoneRotation;
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


// A log row: rotation fingerprint taken from a "pose" seed so two rows of
// the same pose match and rows of different poses do not.
BoneRow LogRow(UInt32 startRegister, UInt32 pose, float tx, float ty, float tz) {
	BoneRow row;
	row.startRegister = startRegister;
	for (UInt32 i = 0; i < kBoneRowFloats; ++i) {
		row.floats[i] = static_cast<float>(i + pose * 100) * 0.125f;
	}
	row.floats[3] = tx;
	row.floats[7] = ty;
	row.floats[11] = tz;
	return row;
}

void TestFingerprint() {
	std::printf("Fingerprint\n");

	const BoneRow a = LogRow(42, 1, 0.0f, 0.0f, 0.0f);
	const BoneRow moved = LogRow(42, 1, 80.0f, 10.0f, 0.0f);
	const BoneRow other = LogRow(42, 2, 0.0f, 0.0f, 0.0f);
	Check(SameBoneRotation(a.floats, a.floats), "a row has its own fingerprint");
	Check(SameBoneRotation(a.floats, moved.floats),
	      "the translation is not part of the fingerprint");
	Check(!SameBoneRotation(a.floats, other.floats), "another pose is another fingerprint");
}

void TestFindBoneRow() {
	std::printf("Find bone row\n");

	// The first render's stream: a guard (pose 1) at 0..1, a same-posed
	// twin guard at 2..3 a body length over, a differently posed body
	// (pose 2) at 4, and another register class at 5.
	BoneRow log[6];
	log[0] = LogRow(42, 1, 0.0f, 0.0f, 0.0f);
	log[1] = LogRow(45, 1, 0.0f, 0.0f, 10.0f);
	log[2] = LogRow(42, 1, 85.0f, 0.0f, 0.0f);
	log[3] = LogRow(45, 1, 85.0f, 0.0f, 10.0f);
	log[4] = LogRow(42, 2, 40.0f, 40.0f, 0.0f);
	log[5] = LogRow(31, 1, 0.0f, 0.0f, 0.0f);

	BoneMatch m = FindBoneRow(log, 0, 0, 42, log[0].floats);
	Check(m.kind == BoneMatchKind::None, "an empty log pairs nothing");

	// Steady frame: the row at the running position, one baseline over.
	const BoneRow eye = LogRow(42, 1, -4.2f, -1.9f, -0.2f);
	m = FindBoneRow(log, 6, 0, 42, eye.floats);
	Check(m.kind == BoneMatchKind::InPlace && m.index == 0,
	      "the running position pairs in place");

	// The engine's mixup: the first guard's draw served the twin's
	// translation. In place, so it is taken - and overwritten - anyway.
	m = FindBoneRow(log, 6, 0, 42, log[2].floats);
	Check(m.kind == BoneMatchKind::InPlace && m.index == 0,
	      "a body length off in place is the mixup and still pairs");

	// The ring slipped by one (the second render skipped a row the first
	// had uploaded): the running position holds the twin, and the incoming
	// row carries the mixup's translation, a body length from either
	// twin. The first fingerprint match along the ring is taken - the
	// translation is what the lock overwrites, not what decides the pair.
	// This is the flow that, refused, brought the collapse back.
	const BoneRow slipped = LogRow(42, 1, 300.0f, -50.0f, 0.0f);
	m = FindBoneRow(log, 6, 1, 42, slipped.floats);
	Check(m.kind == BoneMatchKind::Reordered && m.index == 2,
	      "off position, a row a body length from every twin still pairs with the next");

	// The running position holds another register class; the pair sits
	// further along the ring, the twin first.
	m = FindBoneRow(log, 6, 1, 42, eye.floats);
	Check(m.kind == BoneMatchKind::Reordered && m.index == 2,
	      "off position, the first same-posed row along the ring is the pair");

	// From past the twins the search wraps around to the first guard.
	m = FindBoneRow(log, 6, 4, 42, eye.floats);
	Check(m.kind == BoneMatchKind::Reordered && m.index == 0,
	      "the search wraps around the ring");

	// The running position one past the end wraps to the start.
	m = FindBoneRow(log, 6, 6, 42, eye.floats);
	Check(m.kind == BoneMatchKind::InPlace && m.index == 0,
	      "a running position past the end wraps to the first row");

	// Nothing with the fingerprint at all.
	const BoneRow unknown = LogRow(42, 7, 0.0f, 0.0f, 0.0f);
	m = FindBoneRow(log, 6, 0, 42, unknown.floats);
	Check(m.kind == BoneMatchKind::None, "an unknown fingerprint pairs nothing");

	// The register class is part of the identity: pose 1 at register 31
	// is not the pose-1 row at register 42.
	m = FindBoneRow(log, 6, 5, 42, log[5].floats);
	Check(m.kind == BoneMatchKind::Reordered && m.index == 0,
	      "a register mismatch in place is skipped for the next fingerprint match");
	m = FindBoneRow(log, 6, 5, 31, log[5].floats);
	Check(m.kind == BoneMatchKind::InPlace && m.index == 5,
	      "the hair class pairs in place with its own register");
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
	TestFingerprint();
	TestFindBoneRow();

	if (g_failures != 0) {
		std::printf("%d check(s) FAILED\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
