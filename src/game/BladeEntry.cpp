#include "game/BladeEntry.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/BodyShapes.h"
#include "game/BladeBloodLogic.h"
#include "game/BladeStick.h"
#include "game/NeckBleed.h"
#include "game/Blood.h"
#include "game/CutMarks.h"
#include "game/MeshGapLogic.h"

namespace obvr::game {
namespace {

struct EntryWatch {
	BladeEntryState state;
	BladeEntryNow now;
	UInt32 lines = 300;
	CutMarkState marks;
	UInt32 markLines = 200;
	NiPoint3 edge{1.0f, 0.0f, 0.0f};
	bool sinkSaid = false;
	float deepest = 0.0f;
	BladeBloodState blood;
	UInt32 bloodLines = 200;
	UInt32 bloodActor = 0;
	BodyMaterial bloodMaterial = BodyMaterial::Unknown;
	// Where the stains land (the engine picks the first thing along the
	// ray): the decals under the actor counted before a stain and over the
	// next frames - one more, it is on the body; none, the ground or nothing.
	UInt32 stainActor = 0;
	UInt32 stainDecalsBefore = 0;
	UInt32 stainFrames = 0;
	UInt32 stainsOnBody = 0;
	UInt32 stainsElsewhere = 0;
};

constexpr UInt32 kStainWatchFrames = 4;

// The last stain's landing, judged over the frames after it.
void WatchStainLanding(EntryWatch& w) {
	if (w.stainActor == 0) {
		return;
	}
	const UInt32 now = DecalsUnder(w.stainActor);
	const bool onBody = now > w.stainDecalsBefore;
	if (!onBody && ++w.stainFrames < kStainWatchFrames) {
		return;
	}
	(onBody ? w.stainsOnBody : w.stainsElsewhere) += 1;
	if (w.bloodLines > 0) {
		--w.bloodLines;
		OBVR_LOG("Blade blood: %08X's stain %s (decals under it %u -> %u) - %u on the body, %u elsewhere so far",
		         w.stainActor, onBody ? "landed on the body" : "not on the body (the ground or nothing)",
		         w.stainDecalsBefore, now, w.stainsOnBody, w.stainsElsewhere);
	}
	w.stainActor = 0;
}

EntryWatch& Watch() {
	static EntryWatch w;
	return w;
}

// The angle between two unit directions, in degrees (acos through the atan
// MathFns has).
float DegreesBetween(const NiPoint3& a, const NiPoint3& b) {
	const float c = a.x * b.x + a.y * b.y + a.z * b.z;
	const float clamped = c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c);
	const float s = math::Sqrt(1.0f - clamped * clamped);
	if (clamped > 1e-6f) {
		return math::Atan(s / clamped) / math::kDegreesToRadians;
	}
	if (clamped < -1e-6f) {
		return 180.0f + math::Atan(s / clamped) / math::kDegreesToRadians;
	}
	return 90.0f;
}

const char* KindName(BladeEntryKind k) {
	return k == BladeEntryKind::Stab ? "stab" : (k == BladeEntryKind::Slash ? "slash" : "touch");
}

// The person the blade touches this frame: held at them, or a swing that
// went into them.
bool TouchesPerson(const BladeContactVerdict& v) {
	return v.contact.hit && v.contact.actor != 0 && !IsStopperId(v.contact.actor) && (v.held || v.enteredBody);
}

void Say(EntryWatch& w, const BladeEntryNow& n) {
	if (!n.verdict.fresh || w.lines == 0) {
		return;
	}
	--w.lines;
	const BladeEntryVerdict& e = n.verdict;
	OBVR_LOG("Blade entry: %08X's %s, %s (%u points) at %.2f of the blade, %.0f degrees off square - %s %s%s (%.1f m/s, "
	         "needs %.1f; the blade %.2f %.2f %.2f, the surface %.2f %.2f %.2f)",
	         n.actor, BodyPartName(n.part), BodyMaterialName(n.material), n.rating, static_cast<double>(n.along),
	         static_cast<double>(n.squareDegrees),
	         KindName(e.kind), e.entered ? "goes in" : "stays out: ", e.entered ? (e.byPressure ? " by pressure" : "")
	                                                                            : BladeEntryRefusalName(e.refusal),
	         static_cast<double>(e.speedMetres), static_cast<double>(e.needMetres), static_cast<double>(n.axis.x),
	         static_cast<double>(n.axis.y), static_cast<double>(n.axis.z), static_cast<double>(n.normal.x),
	         static_cast<double>(n.normal.y), static_cast<double>(n.normal.z));
}

// A cut mark where the blade went in (game/CutMarkLogic.h), laid by the
// engine; each said.
void MarkCut(EntryWatch& w, const BladeEntrySettings& settings, const BladeEntryNow& n) {
	CutMarkSettings set;
	set.enabled = settings.cutMarks;
	CutMarkFrame f;
	f.entered = n.verdict.entered;
	f.kind = n.verdict.kind;
	f.material = n.material;
	f.point = n.point;
	f.normal = n.normal;
	f.velocity = n.velocity;
	f.edge = w.edge;
	const CutMark m = StepCutMarks(w.marks, set, f);
	if (!m.place) {
		return;
	}
	const NiPoint3 cut = f.kind == BladeEntryKind::Slash && f.velocity.LengthSquared() > 1e-6f ? f.velocity : f.edge;
	char why[160];
	LayCutMark(n.actor, m, cut, why, sizeof(why));
	if (w.markLines > 0) {
		--w.markLines;
		OBVR_LOG("Cut mark: %08X's %s, %s, mark %u of this %s at %.1f %.1f %.1f (%s) - %s", n.actor, BodyPartName(n.part),
		         BodyMaterialName(n.material), w.marks.marks, KindName(f.kind), static_cast<double>(m.from.x),
		         static_cast<double>(m.from.y), static_cast<double>(m.from.z), m.texture, why);
	}
}

// Blood where the blade went in (game/BladeBloodLogic.h), by the engine's
// own spray and stain (game/Blood.h: none from an actor that does not
// bleed); each said.
void BleedCut(EntryWatch& w, const BladeEntrySettings& settings, const BladeEntryNow& n) {
	BladeBloodSettings set;
	set.enabled = settings.blood;
	BladeBloodFrame f;
	f.entered = n.verdict.entered;
	f.byPressure = n.verdict.byPressure;
	f.kind = n.verdict.kind;
	f.material = n.material;
	f.touching = n.verdict.judged;
	f.point = n.point;
	f.normal = n.normal;
	if (f.touching) {
		w.bloodActor = n.actor;
		w.bloodMaterial = n.material;
	}
	const BloodAct a = StepBladeBlood(w.blood, set, f);
	const UInt32 actor = w.bloodActor;
	WatchStainLanding(w);
	if (a.stain && w.stainActor == 0) {
		w.stainActor = actor;
		w.stainDecalsBefore = DecalsUnder(actor);
		w.stainFrames = 0;
	}
	const bool sprayed = a.spray && SprayBlood(actor, a.sprayAt, a.sprayDir);
	const bool stained = a.stain && StainBlood(actor, a.stainAt, a.stainDir);
	const bool dripped = a.drip && StainBlood(actor, a.dripAt, NiPoint3{0.0f, 0.0f, -1.0f});
	if ((a.spray || a.stain || a.drip) && w.bloodLines > 0) {
		--w.bloodLines;
		OBVR_LOG("Blade blood: %08X - %s%s%s%s (%s)", actor,
		         a.spray ? (a.pulledOut ? "a spurt as the stab comes out " : "a spurt ") : "",
		         a.spray ? (sprayed ? "laid, " : "none, ") : "", a.stain ? (stained ? "a stain laid" : "no stain") : "",
		         a.drip ? (dripped ? ", a drip laid" : ", no drip") : "", BodyMaterialName(w.bloodMaterial));
	}
}

// A slash that went in, held at their surface: sunk toward the hand, to the
// blade's width at most; said once a cut.
void SinkSlash(EntryWatch& w, BladeContactVerdict& v, const BladeContactFrame& f, const BladeEntryNow& n) {
	const bool slashIn = n.verdict.entered && n.verdict.kind == BladeEntryKind::Slash && v.held;
	const float most = BladeSinkUnits(f.section.valid && !f.section.round, f.section.halfWidth);
	const NiPoint3 offset = SlashSinkOffset(slashIn, v.pose.pos, f.wanted.pos, n.normal, most);
	if (!slashIn) {
		w.sinkSaid = false;
		return;
	}
	v.pose.pos = v.pose.pos + offset;
	const float depth = math::Sqrt(offset.LengthSquared());
	w.deepest = depth > w.deepest ? depth : w.deepest;
	if (!w.sinkSaid && depth > 0.5f && w.lines > 0) {
		--w.lines;
		w.sinkSaid = true;
		OBVR_LOG("Blade entry: the slash sinks into %08X's %s %.1f units (at most %.1f, the blade's width)", n.actor,
		         BodyPartName(n.part), static_cast<double>(depth), static_cast<double>(most));
	}
}

}  // namespace

const BladeEntryNow& CurrentBladeEntry() { return Watch().now; }

BladeContactVerdict StepBladeEntryContact(const BladeEntrySettings& settings, BladeContactVerdict v,
                                          const BladeContactFrame& f, const BladePose& from, SInt32 weaponType,
                                          float unitsPerMetre) {
	const bool bladed = weaponType == 0 || weaponType == 1;
	EntryWatch& w = Watch();
	BladeEntryNow n;
	BladeEntryFrame e;
	e.touching = TouchesPerson(v) && f.bodies != nullptr && f.span.valid;
	if (e.touching) {
		n.actor = v.contact.actor;
		n.point = v.contact.point;
		n.normal = v.contact.normal;
		n.along = v.contact.along;
		n.capsule = CapsuleUnderPoint(*f.bodies, n.actor, n.point);
		if (n.capsule >= 0) {
			const BladeBodyCapsule& c = f.bodies->cap[n.capsule];
			n.part = c.part;
			n.material = BodyMaterialAt(c, n.point, &n.rating);
		}
		const NiPoint3 now = BladePointAt(f.wanted, f.span, n.along);
		const NiPoint3 before = BladePointAt(from, f.span, n.along);
		n.velocity = f.dtSeconds > 0.0f ? (now - before) * (1.0f / f.dtSeconds) : NiPoint3{0.0f, 0.0f, 0.0f};
		e.bladed = bladed;
		e.material = n.material;
		e.rating = n.rating;
		e.along = n.along;
		e.axis = Normalised(BladePointAt(v.pose, f.span, 1.0f) - BladePointAt(v.pose, f.span, 0.0f),
		                    NiPoint3{1.0f, 0.0f, 0.0f});
		e.normal = n.normal;
		e.velocity = n.velocity;
		n.squareDegrees = DegreesBetween(e.axis, n.normal * -1.0f);
		n.axis = e.axis;
		e.edgeValid = f.section.valid && !f.section.round;
		e.edge = Normalised(v.pose.rot * f.section.major, NiPoint3{0.0f, 0.0f, 1.0f});
		w.edge = e.edge;
		e.held = v.held;
		e.pressUnits = v.held ? v.gapUnits : 0.0f;
		e.unitsPerMetre = unitsPerMetre;
		e.dtSeconds = f.dtSeconds;
	}
	n.verdict = StepBladeEntry(w.state, settings, e);
	w.now = n;
	Say(w, n);
	MarkCut(w, settings, n);
	BleedCut(w, settings, n);
	SinkSlash(w, v, f, n);
	StepStuckBlade(v, f, n);
	NoteNeckEntry(n, settings.neck);
	return v;
}

}  // namespace obvr::game
