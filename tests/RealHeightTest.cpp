// Checks the real height's decisions (vr/RealHeight.h): the lift taken at a
// recenter from the eyes over the room's floor and the character's own eye,
// its bounds and refusals, the lift a frame applies, and the scale the shown
// body takes for it.

#include <cmath>
#include <cstdio>
#include <limits>

#include "vr/RealHeight.h"

namespace {

using namespace obvr;
using namespace obvr::vr;

int g_failures = 0;

void Check(bool condition, const char* what) {
	std::printf(condition ? "  ok    %s\n" : "  FAIL  %s\n", what);
	if (!condition) {
		++g_failures;
	}
}

bool Near(float a, float b, float within = 0.01f) { return std::fabs(a - b) <= within; }

const float kNan = std::numeric_limits<float>::quiet_NaN();
constexpr float kUnits = 70.0f;

void TestAtRecenter() {
	std::printf("the eyes over the floor at a recenter\n");
	{
		const RealEye e = RealEyeAtRecenter(true, true, 1.85f, kUnits);
		Check(e.known && Near(e.units, 129.5f) && e.why == nullptr, "eyes 1.85 m up: 129.5 units");
	}
	{
		const RealEye e = RealEyeAtRecenter(false, true, 1.85f, kUnits);
		Check(!e.known && e.why != nullptr, "the setting off: not known, said why");
	}
	{
		const RealEye e = RealEyeAtRecenter(true, false, 1.85f, kUnits);
		Check(!e.known && e.why != nullptr, "no floor read: not known");
	}
	Check(!RealEyeAtRecenter(true, true, 1.0f, kUnits).known && !RealEyeAtRecenter(true, true, 2.3f, kUnits).known &&
	          !RealEyeAtRecenter(true, true, kNan, kUnits).known,
	      "eyes outside 1.2 to 2.1 m (sitting, a floor set wrong) or no number: not known");
	Check(RealEyeAtRecenter(true, true, 1.2f, kUnits).known && RealEyeAtRecenter(true, true, 2.1f, kUnits).known,
	      "the ends of the range: known");
	Check(!RealEyeAtRecenter(true, true, 1.85f, 0.0f).known && !RealEyeAtRecenter(true, true, 1.85f, kNan).known,
	      "no units per metre: not known");
}

void TestState() {
	std::printf("the lift taken from the recenter on\n");
	RealHeightState s;
	NoteRealEye(s, RealEyeAtRecenter(true, true, 1.85f, kUnits));
	Check(s.eyeKnown && s.world == 0 && RealHeightLiftNow(s, true, true, false, false) == 0.0f,
	      "after the recenter: the eyes known, no lift until the character is read");
	Check(!RealHeightRetakeDue(s, 0, true, 119.0f), "no world yet (the main menu): not taken");
	Check(!RealHeightRetakeDue(s, 5, false, 119.0f), "the character not standing readable: not taken");
	Check(!RealHeightRetakeDue(s, 5, true, 60.0f) && !RealHeightRetakeDue(s, 5, true, kNan),
	      "its eye no standing one (a sneak) or no number: not taken");
	Check(RealHeightRetakeDue(s, 5, true, 119.0f), "in the world, standing: taken");
	RetakeRealHeight(s, 5, 119.0f, kUnits);
	Check(s.world == 5 && Near(s.liftUnits, 10.5f) && Near(RealHeightLiftNow(s, true, true, false, false), 10.5f),
	      "taken: lifted 10.5 units");
	Check(!RealHeightRetakeDue(s, 5, true, 119.0f), "the same world: not taken again (a jump, a bob changes nothing)");
	Check(RealHeightRetakeDue(s, 6, true, 112.0f), "a load since: taken again (another race)");
	RetakeRealHeight(s, 6, 112.0f, kUnits);
	Check(Near(s.liftUnits, 17.5f), "against the new character's eye");
	NoteRealEye(s, RealEyeAtRecenter(false, true, 1.85f, kUnits));
	Check(!s.eyeKnown && s.liftUnits == 0.0f && !RealHeightRetakeDue(s, 7, true, 119.0f) &&
	          RealHeightLiftNow(s, true, true, false, false) == 0.0f,
	      "a recenter refused: no lift, nothing taken");
}

void TestAgain() {
	std::printf("the lift taken again for another character eye\n");
	Check(Near(RealHeightLift(129.5f, 119.0f, kUnits), 10.5f), "the same real eye over another character eye");
	Check(Near(RealHeightLift(200.0f, 100.0f, kUnits), kRealHeightMostMetres * kUnits), "held within half a metre");
	Check(RealHeightLift(129.5f, kNan, kUnits) == 0.0f && RealHeightLift(kNan, 119.0f, kUnits) == 0.0f,
	      "no number: no lift");
	Check(RealHeightLift(129.5f, 30.0f, kUnits) == 0.0f, "an implausible character eye: no lift");
}

void TestNow() {
	std::printf("the lift a frame applies\n");
	RealHeightState s;
	NoteRealEye(s, RealEyeAtRecenter(true, true, 1.85f, kUnits));
	RetakeRealHeight(s, 3, 119.0f, kUnits);
	Check(Near(RealHeightLiftNow(s, true, true, false, false), 10.5f), "first person, on foot: the lift");
	Check(RealHeightLiftNow(s, false, true, false, false) == 0.0f, "the setting off since: nothing");
	Check(RealHeightLiftNow(s, true, false, false, false) == 0.0f, "third person: the chase camera's own height");
	Check(RealHeightLiftNow(s, true, true, true, false) == 0.0f, "riding: the saddle's own height");
	Check(RealHeightLiftNow(s, true, true, false, true) == 0.0f, "the death view: its own height");
	s.liftUnits = kNan;
	Check(RealHeightLiftNow(s, true, true, false, false) == 0.0f, "no number: nothing");
}

void TestBodyScale() {
	std::printf("the shown body's scale\n");
	Check(Near(RealHeightBodyScale(10.5f, 119.0f), 129.5f / 119.0f, 1e-4f), "the real eye over the character's");
	Check(RealHeightBodyScale(0.0f, 119.0f) == 1.0f, "no lift: as it is");
	Check(Near(RealHeightBodyScale(-14.0f, 119.0f), 105.0f / 119.0f, 1e-4f), "let down: smaller");
	Check(RealHeightBodyScale(60.0f, 120.0f) == kRealHeightBodyScaleMost &&
	          RealHeightBodyScale(-50.0f, 160.0f) == kRealHeightBodyScaleLeast,
	      "held within its bounds");
	Check(RealHeightBodyScale(10.0f, kNan) == 1.0f && RealHeightBodyScale(kNan, 119.0f) == 1.0f &&
	          RealHeightBodyScale(10.0f, 30.0f) == 1.0f,
	      "no number or an implausible eye: as it is");
}

void TestBaseScale() {
	std::printf("the body's own scale under the one written\n");
	Check(Near(BaseScaleNow(1.0f, 1.09f, 1.09f), 1.0f), "it reads what was written: its own is the one kept");
	Check(Near(BaseScaleNow(1.0f, 1.09f, 1.03f), 1.03f), "the engine wrote another: that is its own now");
	Check(Near(BaseScaleNow(0.0f, 0.0f, 1.0f), 1.0f), "nothing written yet: what it reads");
	Check(Near(BaseScaleNow(1.0f, 1.09f, kNan), 1.0f) && Near(BaseScaleNow(1.0f, 1.09f, -2.0f), 1.0f),
	      "a reading that is no scale: the kept one");
}

}  // namespace

int main() {
	TestAtRecenter();
	TestState();
	TestAgain();
	TestNow();
	TestBodyScale();
	TestBaseScale();
	if (g_failures > 0) {
		std::printf("%d check(s) failed\n", g_failures);
		return 1;
	}
	std::printf("all checks passed\n");
	return 0;
}
