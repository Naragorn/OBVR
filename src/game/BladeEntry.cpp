#include "game/BladeEntry.h"

#include "core/Log.h"
#include "game/BodyShapes.h"
#include "game/MeshGapLogic.h"

namespace obvr::game {
namespace {

struct EntryWatch {
	BladeEntryState state;
	BladeEntryNow now;
	UInt32 lines = 300;
};

EntryWatch& Watch() {
	static EntryWatch w;
	return w;
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
	OBVR_LOG("Blade entry: %08X's %s, %s (%u points) at %.2f of the blade - %s %s%s (%.1f m/s, needs %.1f)", n.actor,
	         BodyPartName(n.part), BodyMaterialName(n.material), n.rating, static_cast<double>(n.along),
	         KindName(e.kind), e.entered ? "goes in" : "stays out: ", e.entered ? (e.byPressure ? " by pressure" : "")
	                                                                            : BladeEntryRefusalName(e.refusal),
	         static_cast<double>(e.speedMetres), static_cast<double>(e.needMetres));
}

}  // namespace

const BladeEntryNow& StepBladeEntryContact(const BladeEntrySettings& settings, const BladeContactVerdict& v,
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
		e.edgeValid = f.section.valid && !f.section.round;
		e.edge = Normalised(v.pose.rot * f.section.major, NiPoint3{0.0f, 0.0f, 1.0f});
		e.held = v.held;
		e.pressUnits = v.held ? v.gapUnits : 0.0f;
		e.unitsPerMetre = unitsPerMetre;
		e.dtSeconds = f.dtSeconds;
	}
	n.verdict = StepBladeEntry(w.state, settings, e);
	w.now = n;
	Say(w, n);
	return w.now;
}

}  // namespace obvr::game
