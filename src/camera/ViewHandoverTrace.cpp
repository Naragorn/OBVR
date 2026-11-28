#include "camera/ViewHandoverTrace.h"

#include "camera/CameraHook.h"
#include "camera/ViewHandover.h"
#include "core/Log.h"
#include "vr/HeadTracker.h"
#include "vr/OpenVRBackend.h"
#include "vr/OpenVRTypes.h"

namespace obvr::camera {

namespace {

// The one owner of the state (the size ratchet allows this file no globals).
struct ViewTraceOwner {
	WorldViewTerms worldTerms;  // the last world frame's
	ViewBuild pending;          // this frame's, consumed at the render
	ViewHandoverState handover;
};

ViewTraceOwner& Owner() {
	static ViewTraceOwner s_owner;
	return s_owner;
}

// The head pose's source and the headset's tracked place, for the build.
void NotePose(ViewBuild& b) {
	vr::OpenVRBackend& backend = GetHeadTracker().GetBackendForFrame();
	b.posePredicted = backend.FramePosesPredicted();
	vr::openvr::HmdMatrix34 pose{};
	b.headTracked = backend.GetRenderPoseMatrix(pose) ? NiPoint3{pose.m[0][3], pose.m[1][3], pose.m[2][3]}
	                                                  : NiPoint3{0.0f, 0.0f, 0.0f};
}

double X(const NiPoint3& p) { return static_cast<double>(p.x); }
double Y(const NiPoint3& p) { return static_cast<double>(p.y); }
double Z(const NiPoint3& p) { return static_cast<double>(p.z); }

// The frame as drawn and what it was built from.
void LogFrame(const ViewBuild& b, const ViewHandoverVerdict& v) {
	const NiPoint3 built = BuiltFirstEye(b);
	const NiPoint3 second = b.eyeDrawn + b.shift;
	OBVR_LOG("View handover: %s frame %u after the change from the %s - the first eye drawn at %.2f %.2f %.2f (built "
	         "%.2f %.2f %.2f), the second at %.2f %.2f %.2f, the cyclopean %.2f %.2f %.2f; the pose %s, the headset at "
	         "%.3f %.3f %.3f m; built on the base %.2f %.2f %.2f + arc %.2f %.2f %.2f + tilt %.2f %.2f %.2f + anim %.2f "
	         "%.2f %.2f + head %.2f %.2f %.2f + eye step %.2f %.2f %.2f, height %.2f + real lift %.2f, shift %.2f %.2f %.2f",
	         ViewFrameKindName(b.kind), v.frameIndex, ViewFrameKindName(v.other.kind), X(b.eyeDrawn), Y(b.eyeDrawn),
	         Z(b.eyeDrawn), X(built), Y(built), Z(built), X(second), Y(second), Z(second), X(b.cyclopean), Y(b.cyclopean),
	         Z(b.cyclopean), b.posePredicted ? "predicted" : "waited for", X(b.headTracked), Y(b.headTracked),
	         Z(b.headTracked), X(b.base), Y(b.base), Z(b.base), X(b.aimArc), Y(b.aimArc), Z(b.aimArc), X(b.aimTilt),
	         Y(b.aimTilt), Z(b.aimTilt), X(b.engineAnim), Y(b.engineAnim), Z(b.engineAnim), X(b.head), Y(b.head),
	         Z(b.head), X(b.firstEyeStep), Y(b.firstEyeStep), Z(b.firstEyeStep), static_cast<double>(b.height),
	         static_cast<double>(b.realLift), X(b.shift), Y(b.shift), Z(b.shift));
}

// Every term's difference against the other kind's last frame, in this
// frame's head axes (right, forward, up).
void LogDifference(const ViewBuild& b, const ViewHandoverVerdict& v) {
	const ViewBuild& o = v.other;
	const auto d = [&b](const NiPoint3& now, const NiPoint3& then) { return InHeadAxes(b.view, now - then); };
	const NiPoint3 eye = d(b.eyeDrawn, o.eyeDrawn);
	const NiPoint3 cyc = d(b.cyclopean, o.cyclopean);
	const NiPoint3 base = d(b.base, o.base);
	const NiPoint3 arc = d(b.aimArc, o.aimArc);
	const NiPoint3 tilt = d(b.aimTilt, o.aimTilt);
	const NiPoint3 anim = d(b.engineAnim, o.engineAnim);
	const NiPoint3 head = d(b.head, o.head);
	const NiPoint3 step = d(b.firstEyeStep, o.firstEyeStep);
	const NiPoint3 shift = d(b.shift, o.shift);
	const NiPoint3 offNow = d(b.eyeDrawn, BuiltFirstEye(b));
	const NiPoint3 offThen = d(o.eyeDrawn, BuiltFirstEye(o));
	OBVR_LOG("View handover: against the last %s frame, in the head's right, forward, up - the first eye drawn %+.2f "
	         "%+.2f %+.2f (%.2f half-IPDs), the cyclopean %+.2f %+.2f %+.2f (%.2f); by term: base %+.2f %+.2f %+.2f, arc "
	         "%+.2f %+.2f %+.2f, tilt %+.2f %+.2f %+.2f, anim %+.2f %+.2f %+.2f, head %+.2f %+.2f %+.2f, eye step %+.2f "
	         "%+.2f %+.2f, shift %+.2f %+.2f %+.2f, height %+.2f, real lift %+.2f; drawn minus built %+.2f %+.2f %+.2f "
	         "(then %+.2f %+.2f %+.2f); the pose then %s",
	         ViewFrameKindName(o.kind), X(eye), Y(eye), Z(eye), static_cast<double>(HalfIpds(eye, b.shift)), X(cyc),
	         Y(cyc), Z(cyc), static_cast<double>(HalfIpds(cyc, b.shift)), X(base), Y(base), Z(base), X(arc), Y(arc),
	         Z(arc), X(tilt), Y(tilt), Z(tilt), X(anim), Y(anim), Z(anim), X(head), Y(head), Z(head), X(step), Y(step),
	         Z(step), X(shift), Y(shift), Z(shift), static_cast<double>(b.height - o.height),
	         static_cast<double>(b.realLift - o.realLift), X(offNow), Y(offNow), Z(offNow), X(offThen), Y(offThen),
	         Z(offThen), o.posePredicted ? "predicted" : "waited for");
}

}  // namespace

const WorldViewTerms& LastWorldViewTerms() { return Owner().worldTerms; }

void NoteWorldView(const WorldViewTerms& terms, const NiPoint3& engineEye, const NiPoint3& head, float realLift) {
	ViewTraceOwner& o = Owner();
	o.worldTerms = terms;
	ViewBuild& b = o.pending;
	b = ViewBuild{};
	b.kind = ViewFrameKind::Hook;
	b.base = engineEye;
	b.aimArc = terms.aimArc;
	b.aimTilt = terms.aimTilt;
	b.engineAnim = terms.engineAnim;
	b.height = terms.lookHeight + terms.sneakLift;
	b.realLift = realLift;
	b.head = head;
	NotePose(b);
}

void NoteViewSteps(const NiMatrix33& view, const NiPoint3& firstEyeStep, const NiPoint3& shift) {
	ViewBuild& b = Owner().pending;
	b.view = view;
	b.firstEyeStep = firstEyeStep;
	b.shift = shift;
}

void NoteMenuView(const NiPoint3& placedBase, bool termsUsed, float height, float realLift, const NiPoint3& head,
                  const NiPoint3& firstEyeStep, const NiPoint3& shift, const NiMatrix33& view) {
	ViewTraceOwner& o = Owner();
	ViewBuild& b = o.pending;
	b = ViewBuild{};
	b.kind = ViewFrameKind::StandIn;
	if (termsUsed) {
		b.aimArc = o.worldTerms.aimArc;
		b.aimTilt = o.worldTerms.aimTilt;
		b.engineAnim = o.worldTerms.engineAnim;
	}
	b.base = placedBase - b.aimArc - b.aimTilt - b.engineAnim;
	b.height = height - realLift;
	b.realLift = realLift;
	b.head = head;
	b.firstEyeStep = firstEyeStep;
	b.shift = shift;
	b.view = view;
	NotePose(b);
}

void TraceViewAsDrawn(const NiPoint3& eyeDrawn, const NiPoint3& cyclopean) {
	ViewTraceOwner& o = Owner();
	ViewBuild& b = o.pending;
	b.eyeDrawn = eyeDrawn;
	b.cyclopean = cyclopean;
	const ViewHandoverVerdict v = StepViewHandover(o.handover, b);
	if (v.log) {
		LogFrame(b, v);
		LogDifference(b, v);
	}
	b.kind = ViewFrameKind::None;
}

}  // namespace obvr::camera
