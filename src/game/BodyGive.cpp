#include "game/BodyGive.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BladeBodies.h"
#include "game/BoneWrite.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/Shove.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// As far from the eyes as the hands' own contact looks for people.
constexpr float kGiveWithinUnits = 130.0f;
constexpr UInt32 kGiveNearMost = 24;
// How many people give at once: the ones in reach of two hands and a blade.
constexpr UInt32 kGiveActorsMost = 6;
// A joint turned past this is said once a touch (the harness's evidence).
constexpr float kGiveSaidDegrees = 5.0f;

// The bone each arm joint turns (BladeBone); the neck's is found from the
// head (HeadJoint).
constexpr UInt32 kJointBone[kGiveJointCount] = {kBoneHead, kBoneLUpperArm, kBoneLForearm, kBoneRUpperArm, kBoneRForearm};
const char* const kJointName[kGiveJointCount] = {"neck", "left shoulder", "left elbow", "right shoulder", "right elbow"};
const char* const kPusherName[kGivePushers] = {"the weapon hand", "the other hand", "the blade"};

struct Giving {
	UInt32 actor = 0;
	const NiAVObject* root = nullptr;
	GiveState state;
	BoneWrite write[kGiveJointCount];
	bool seen = false;
	bool saidTurned = false;
	float seconds = 0.0f;
	// How far the head bone or a wrist stands from where the animation had
	// it, after this frame's writes: the turn reached the bones under it.
	float movedUnits = 0.0f;
};

struct GiveMemory {
	Giving who[kGiveActorsMost];
	UInt32 linesLeft = 60;
	// The step's cost between two marks.
	double costSeconds = 0.0;
	double worstSeconds = 0.0;
	UInt32 costFrames = 0;
	UInt32 nearMost = 0;
};

GiveMemory& Memory() {
	static GiveMemory s_memory;
	return s_memory;
}

bool Readable(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }

// The actor's reference id, for the log.
UInt32 RefIdOf(UInt32 actor) { return *reinterpret_cast<const UInt32*>(actor + addr::kFormIdOffset); }

Giving* Find(GiveMemory& m, UInt32 actor, const NiAVObject* root) {
	for (Giving& g : m.who) {
		if (g.actor == actor && g.root == root && actor != 0) {
			return &g;
		}
	}
	return nullptr;
}

Giving* FreeSlot(GiveMemory& m) {
	for (Giving& g : m.who) {
		if (g.actor == 0) {
			return &g;
		}
	}
	return nullptr;
}

// A bone kept by the contact's reading (game/BladeBodies.h), to write to:
// with a parent, as every bone under the skeleton's root has.
NiAVObject* Bone(const LivingRef& who, UInt32 bone) {
	auto* const node =
		const_cast<NiAVObject*>(ActorBoneNode(who.actor, reinterpret_cast<UInt32>(who.root), bone));
	return node != nullptr && Readable(node->parent) ? node : nullptr;
}

struct GiveNodes {
	NiAVObject* joint[kGiveJointCount] = {};
	NiAVObject* head = nullptr;
	NiAVObject* hand[2] = {};
};

// The bone the head turns about (HeadTurnsAtParent): the head's parent when
// it carries the head alone, else the head.
NiAVObject* HeadJoint(NiAVObject* head) {
	if (head == nullptr) {
		return nullptr;
	}
	NiAVObject* const parent = head->parent;
	UInt32 children = 0;
	const bool bone = Readable(parent) && Readable(parent->parent);
	if (bone) {
		NodeChildren(parent, children);
	}
	return HeadTurnsAtParent(bone, children) ? parent : head;
}

GiveNodes NodesOf(const LivingRef& who) {
	GiveNodes n;
	for (UInt32 j = kGiveLeftUpper; j < kGiveJointCount; ++j) {
		n.joint[j] = Bone(who, kJointBone[j]);
	}
	n.head = Bone(who, kBoneHead);
	n.joint[kGiveNeck] = HeadJoint(n.head);
	n.hand[0] = Bone(who, kBoneLHand);
	n.hand[1] = Bone(who, kBoneRHand);
	return n;
}

// The bones as the animation has them: what was written the frame before is
// given back where the engine left it, and the bones under it are placed
// again, so a body the engine did not animate this frame is not read turned.
void BeginBones(Giving& g, const GiveNodes& n) {
	UInt32 kept = 0;
	UInt32 renewed = 0;
	bool wrote[kGiveJointCount];
	for (UInt32 j = 0; j < kGiveJointCount; ++j) {
		wrote[j] = g.write[j].held;
		BeginWrite(g.write[j], n.joint[j], kept, renewed);
	}
	if (wrote[kGiveNeck] && n.joint[kGiveNeck] != nullptr) {
		UpdateNodeTransforms(n.joint[kGiveNeck]);
	}
	for (UInt32 arm = 0; arm < 2; ++arm) {
		const UInt32 upper = GiveUpperJoint(arm);
		const UInt32 fore = GiveForeJoint(arm);
		if (wrote[upper] && n.joint[upper] != nullptr) {
			UpdateNodeTransforms(n.joint[upper]);  // the forearm under it with it
		} else if (wrote[fore] && n.joint[fore] != nullptr) {
			UpdateNodeTransforms(n.joint[fore]);
		}
	}
}

GiveBones BonesOf(const LivingRef& who, const GiveNodes& n) {
	GiveBones b;
	b.scale = who.root->worldTransform.scale;
	b.head = n.joint[kGiveNeck] != nullptr && n.head != nullptr;
	if (b.head) {
		b.neck = n.joint[kGiveNeck]->worldTransform.pos;
		b.headBone = n.head->worldTransform.pos;
	}
	for (UInt32 arm = 0; arm < 2; ++arm) {
		const NiAVObject* const upper = n.joint[GiveUpperJoint(arm)];
		const NiAVObject* const fore = n.joint[GiveForeJoint(arm)];
		b.arm[arm] = upper != nullptr && fore != nullptr && n.hand[arm] != nullptr;
		if (b.arm[arm]) {
			b.shoulder[arm] = upper->worldTransform.pos;
			b.elbow[arm] = fore->worldTransform.pos;
			b.wrist[arm] = n.hand[arm]->worldTransform.pos;
		}
	}
	return b;
}

// One joint's bone turned on top of the animation, and everything under it
// placed again. A joint at rest is left to the animation.
void WriteJoint(Giving& g, UInt32 joint, NiAVObject* node) {
	const NiPoint3& turn = g.state.joint[joint].turn;
	if (node == nullptr || !(turn.LengthSquared() > 0.0f)) {
		return;
	}
	NiTransform local = node->localTransform;
	local.rot = TurnedLocalRot(node->parent->worldTransform.rot, node->worldTransform.rot, turn);
	WriteLocal(g.write[joint], local);
	UpdateNodeTransforms(node);
}

const char* TouchedPart(UInt32 joint) {
	if (joint == kGiveNeck) {
		return "head";
	}
	return joint == kGiveLeftUpper || joint == kGiveLeftFore ? "left arm" : "right arm";
}

void SayTouched(GiveMemory& m, const Giving& g) {
	if (m.linesLeft == 0) {
		return;
	}
	for (UInt32 j = 0; j < kGiveJointCount; ++j) {
		const UInt32 by = g.state.joint[j].touchedBy;
		if (by == 0) {
			continue;
		}
		UInt32 pusher = 0;
		while (pusher + 1 < kGivePushers && (by & (1u << pusher)) == 0) {
			++pusher;
		}
		--m.linesLeft;
		OBVR_LOG("Body give: %08X gives - touched at the %s by %s, its %s turning at %.1f radians a second", RefIdOf(g.actor),
		         TouchedPart(j), kPusherName[pusher], kJointName[j], static_cast<double>(ik::Length(g.state.joint[j].speed)));
		return;
	}
}

void SayTurned(GiveMemory& m, Giving& g) {
	if (g.saidTurned || m.linesLeft == 0) {
		return;
	}
	const float said = kGiveSaidDegrees * math::kDegreesToRadians;
	for (UInt32 j = 0; j < kGiveJointCount; ++j) {
		if (g.state.joint[j].turn.LengthSquared() > said * said) {
			g.saidTurned = true;
			--m.linesLeft;
			OBVR_LOG("Body give: %08X - its %s turned past %.0f degrees; the bone under it stands %.1f units from "
			         "where the animation has it (%s)",
			         RefIdOf(g.actor), kJointName[j], static_cast<double>(kGiveSaidDegrees),
			         static_cast<double>(g.movedUnits), g.movedUnits > 0.1f ? "the bones moved" : "THE BONES DID NOT MOVE");
			return;
		}
	}
}

// The furthest of the head bone and the wrists from where the animation had
// them (`bones`), read after the writes.
float MovedUnits(const GiveBones& bones, const GiveNodes& n) {
	float most = 0.0f;
	if (bones.head) {
		most = ik::Length(n.head->worldTransform.pos - bones.headBone);
	}
	for (UInt32 arm = 0; arm < 2; ++arm) {
		if (bones.arm[arm]) {
			const float moved = ik::Length(n.hand[arm]->worldTransform.pos - bones.wrist[arm]);
			most = moved > most ? moved : most;
		}
	}
	return most;
}

void StepOne(GiveMemory& m, const LivingRef& who, const GiveFrame& f) {
	Giving* g = Find(m, who.actor, who.root);
	if (g == nullptr && !PusherNear(f.pusher, kGivePushers, who.centre, who.radius)) {
		return;
	}
	// Lying or getting up, Havok or the get-up has the bones: let go (the
	// entry, left unseen, is dropped).
	if (ActorKnockState(reinterpret_cast<void*>(who.actor)) != 0) {
		return;
	}
	const GiveNodes nodes = NodesOf(who);
	if (g != nullptr) {
		BeginBones(*g, nodes);
	}
	const GiveBones bones = BonesOf(who, nodes);
	GiveState fresh;
	const bool known = g != nullptr;
	if (!StepGive(known ? g->state : fresh, bones, f.pusher, kGivePushers, f.dtSeconds)) {
		if (known) {
			if (m.linesLeft > 0) {
				--m.linesLeft;
				OBVR_LOG("Body give: %08X at rest again after %.2f s", RefIdOf(g->actor), static_cast<double>(g->seconds));
			}
			*g = Giving{};
		}
		return;
	}
	if (!known) {
		g = FreeSlot(m);
		if (g == nullptr) {
			return;  // six people give already
		}
		g->actor = who.actor;
		g->root = who.root;
		g->state = fresh;
		BeginBones(*g, nodes);  // nothing was written yet: the bones are only booked
		SayTouched(m, *g);
	}
	g->seen = true;
	g->seconds += f.dtSeconds;
	WriteJoint(*g, kGiveNeck, nodes.joint[kGiveNeck]);
	for (UInt32 arm = 0; arm < 2; ++arm) {
		WriteJoint(*g, GiveUpperJoint(arm), nodes.joint[GiveUpperJoint(arm)]);
		WriteJoint(*g, GiveForeJoint(arm), nodes.joint[GiveForeJoint(arm)]);
	}
	g->movedUnits = MovedUnits(bones, nodes);
	SayTurned(m, *g);
}

void SayAtMark(GiveMemory& m) {
	bool any = false;
	for (const Giving& g : m.who) {
		if (g.actor == 0) {
			continue;
		}
		any = true;
		float degrees[kGiveJointCount];
		for (UInt32 j = 0; j < kGiveJointCount; ++j) {
			degrees[j] = ik::Length(g.state.joint[j].turn) * math::kRadiansToDegrees;
		}
		OBVR_LOG("Body give: at the mark - %08X gives: the neck %.1f degrees, the left shoulder %.1f, the left elbow "
		         "%.1f, the right shoulder %.1f, the right elbow %.1f; a bone under them %.1f units from the animation's",
		         RefIdOf(g.actor), static_cast<double>(degrees[kGiveNeck]), static_cast<double>(degrees[kGiveLeftUpper]),
		         static_cast<double>(degrees[kGiveLeftFore]), static_cast<double>(degrees[kGiveRightUpper]),
		         static_cast<double>(degrees[kGiveRightFore]), static_cast<double>(g.movedUnits));
	}
	if (!any) {
		OBVR_LOG("Body give: at the mark - nobody gives");
	}
	if (m.costFrames > 0) {
		OBVR_LOG("Body give: %u frames since the last mark - %.4f ms a frame on average, the worst %.4f ms; %u people "
		         "near at the most",
		         m.costFrames, 1000.0 * m.costSeconds / static_cast<double>(m.costFrames), 1000.0 * m.worstSeconds,
		         m.nearMost);
	}
	m.costSeconds = 0.0;
	m.worstSeconds = 0.0;
	m.costFrames = 0;
	m.nearMost = 0;
}

// At a mark: where the nearest of the living has the bones that give, and
// the eyes - what a scenario's hand has to be aimed at.
void SayNearestAtMark(const GiveFrame& f, const LivingRef* living, UInt32 people) {
	const LivingRef* nearest = nullptr;
	for (UInt32 i = 0; i < people; ++i) {
		if (nearest == nullptr ||
		    (living[i].centre - f.around).LengthSquared() < (nearest->centre - f.around).LengthSquared()) {
			nearest = &living[i];
		}
	}
	if (nearest == nullptr) {
		return;
	}
	const GiveBones b = BonesOf(*nearest, NodesOf(*nearest));
	OBVR_LOG("Body give: at the mark - the eyes at %.0f %.0f %.0f; the nearest, %08X: the head turns at %.0f %.0f %.0f, "
	         "its bone at %.0f %.0f %.0f; left shoulder %.0f %.0f %.0f, elbow %.0f %.0f %.0f, wrist %.0f %.0f %.0f; "
	         "right shoulder %.0f %.0f %.0f, elbow %.0f %.0f %.0f, wrist %.0f %.0f %.0f (read: head %d, arms %d %d)",
	         static_cast<double>(f.around.x), static_cast<double>(f.around.y), static_cast<double>(f.around.z),
	         RefIdOf(nearest->actor), static_cast<double>(b.neck.x), static_cast<double>(b.neck.y),
	         static_cast<double>(b.neck.z), static_cast<double>(b.headBone.x), static_cast<double>(b.headBone.y),
	         static_cast<double>(b.headBone.z), static_cast<double>(b.shoulder[0].x), static_cast<double>(b.shoulder[0].y),
	         static_cast<double>(b.shoulder[0].z), static_cast<double>(b.elbow[0].x), static_cast<double>(b.elbow[0].y),
	         static_cast<double>(b.elbow[0].z), static_cast<double>(b.wrist[0].x), static_cast<double>(b.wrist[0].y),
	         static_cast<double>(b.wrist[0].z), static_cast<double>(b.shoulder[1].x), static_cast<double>(b.shoulder[1].y),
	         static_cast<double>(b.shoulder[1].z), static_cast<double>(b.elbow[1].x), static_cast<double>(b.elbow[1].y),
	         static_cast<double>(b.elbow[1].z), static_cast<double>(b.wrist[1].x), static_cast<double>(b.wrist[1].y),
	         static_cast<double>(b.wrist[1].z), b.head ? 1 : 0, b.arm[0] ? 1 : 0, b.arm[1] ? 1 : 0);
	OBVR_LOG("Body give: at the mark - its knock state %d, its bound %.0f units round %.0f %.0f %.0f, a pusher %s it",
	         ActorKnockState(reinterpret_cast<void*>(nearest->actor)), static_cast<double>(nearest->radius),
	         static_cast<double>(nearest->centre.x), static_cast<double>(nearest->centre.y),
	         static_cast<double>(nearest->centre.z),
	         PusherNear(f.pusher, kGivePushers, nearest->centre, nearest->radius) ? "near" : "NOT near");
	for (UInt32 p = 0; p < kGivePushers; ++p) {
		const GivePusher& pusher = f.pusher[p];
		if (pusher.valid) {
			OBVR_LOG("Body give: at the mark - %s from %.0f %.0f %.0f to %.0f %.0f %.0f, %.1f round", kPusherName[p],
			         static_cast<double>(pusher.a.x), static_cast<double>(pusher.a.y), static_cast<double>(pusher.a.z),
			         static_cast<double>(pusher.b.x), static_cast<double>(pusher.b.y), static_cast<double>(pusher.b.z),
			         static_cast<double>(pusher.radius));
		}
	}
}

bool AnyoneGiving(const GiveMemory& m) {
	for (const Giving& g : m.who) {
		if (g.actor != 0) {
			return true;
		}
	}
	return false;
}

bool AnyPusher(const GiveFrame& f) {
	for (const GivePusher& p : f.pusher) {
		if (p.valid) {
			return true;
		}
	}
	return false;
}

}  // namespace

void StepBodyGive(const GiveFrame& frame) {
	GiveMemory& m = Memory();
	if (!frame.enabled) {
		// The bones written were given back after the last draw: only the
		// bookings go, and nothing that may be gone by now is read.
		if (AnyoneGiving(m)) {
			for (Giving& g : m.who) {
				g = Giving{};
			}
		}
		return;
	}
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const long long before = ReadPerformanceCounter();
	UInt32 people = 0;
	if (AnyPusher(frame) || AnyoneGiving(m)) {
		LivingRef living[kGiveNearMost];
		people = LivingNear(frame.around, kGiveWithinUnits, living, kGiveNearMost);
		if (frame.mark) {
			SayNearestAtMark(frame, living, people);
		}
		for (Giving& g : m.who) {
			g.seen = false;
		}
		for (UInt32 i = 0; i < people; ++i) {
			StepOne(m, living[i], frame);
		}
		// Whoever was not stepped is gone, dead, lying, or out of reach: let
		// go without a read (their bones were given back after the draw).
		for (Giving& g : m.who) {
			if (g.actor != 0 && !g.seen) {
				g = Giving{};
			}
		}
	}
	if (ticksPerSecond > 0) {
		const double seconds = static_cast<double>(ReadPerformanceCounter() - before) / static_cast<double>(ticksPerSecond);
		m.costSeconds += seconds;
		m.worstSeconds = seconds > m.worstSeconds ? seconds : m.worstSeconds;
		++m.costFrames;
		m.nearMost = people > m.nearMost ? people : m.nearMost;
	}
	if (frame.mark) {
		SayAtMark(m);
	}
}

void GiveBackBodyGive() {
	for (Giving& g : Memory().who) {
		if (g.actor == 0) {
			continue;
		}
		for (BoneWrite& w : g.write) {
			GiveBackAfterDraw(w);
		}
	}
}

}  // namespace obvr::game
