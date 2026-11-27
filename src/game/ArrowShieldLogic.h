#pragma once

// Arrows by the shield's contact rule ([Hands] ShieldBlockByContact; Nadi,
// 2026-11-27, "go": an arrow that hits the shield is blocked and shows the
// shield's impact instead of the body's).
//
// Read in Oblivion.exe 1.2.0.416 (dumpbin and ../OBNG/re/export/rea-all/
// functions/0x60a640.c, 2026-11-27): the Havok contact callback 0x0060D950
// calls the projectile's impact with an actor, 0x0060A640 (thiscall, ecx the
// projectile; args: the contact point in game units, the contact normal, the
// actor hit). It fills the projectile's hit record (+0x5C, kind 0) and
// picks where the arrow is attached: the shield's node - 0x008AFB50(root, 7),
// the first node whose collision filter carries part 7 - when the target is
// in the Block action (0x005E5670 at 0x0060AD01), has a shield (process
// vtable +0xF8), the cone 0x006131D0(target, projectile) says in front and the
// point is at or above half the target's height (0x005E0660); else the body
// node nearest the point (0x008AFD70). Both paths meet at 0x0060AD8B with al
// the answer and edi the point. The damage comes later, in the hit handler
// 0x005FEBF0 for the same projectile (OBVR's block check and cone, game/Parry,
// game/BlockCone).
//
// With the shield by contact nothing holds Block, so that test always said
// "body". Now the call at 0x0060AD01 goes through a stub (game/ArrowShield.cpp,
// BuildArrowShieldStub) that asks ArrowAtShieldFor first: Blocked takes the
// shield's branch, Through the body's, AsRaised the vanilla test with the
// Block answer the old pose gives, Vanilla the original call untouched. The
// verdict is kept for the projectile (ArrowVerdicts) and the hit handler's
// cone takes it for the same projectile - one decision for the effect and the
// damage.
//
// Pure; arrow_shield_test.

#include <cmath>

#include "core/Types.h"
#include "game/BladeContactLogic.h"
#include "game/NiMath.h"
#include "game/ShieldOutlineLogic.h"

namespace obvr::game {

// What the stub does at 0x0060AD01 (the callback's return, eax).
enum class ArrowAtShield : UInt32 {
	Vanilla = 0,   // the original block check, untouched (not the player, the rule off)
	Blocked = 1,   // the shield's branch: al = 1 at 0x0060AD80
	Through = 2,   // the body's branch: 0x0060AD84
	AsRaised = 3,  // the vanilla test goes on, its Block answered by the old pose (the disc not judged)
};

// The margins (guessed until the headset; the log prints the distances):
// - in the disc's plane, past its rim - only for the circle, when the
//   shield's outline was not read (ShieldOutlineLogic.h; with it the arrow's
//   half width is the margin, kArrowHalfWidthUnits): an arrow's head and
//   fletching are a few units across, and the circle is the longer spoke's;
// - along its normal, past its half thickness: the contact point is where
//   the arrow's body met the target's collision, which may lie a step
//   behind the shield (the step is not read - guessed);
// - the segment from the projectile's last position to the point: crossing
//   the disc grown by a little, the arrow's half width.
inline constexpr float kArrowDiscRimMarginUnits = 6.0f;
inline constexpr float kArrowDiscFaceMarginUnits = 10.0f;
inline constexpr float kArrowSegmentMarginUnits = 3.0f;
// A segment shorter than this is no direction.
inline constexpr float kArrowSegmentMinUnits = 1.0f;
// With the outline read, the arrow's half width is the only margin in the
// shield's plane (guessed: an arrow's shaft and head about 3 units across;
// its bound was not read).
inline constexpr float kArrowHalfWidthUnits = 1.5f;

struct ArrowShieldInput {
	bool targetIsPlayer = false;
	bool byContact = false;  // ShieldGuardPose::byContact this frame
	bool raised = false;     // ShieldGuardPose::raised: the old pose
	bool discValid = false;  // the disc placed in the world this frame
	BladeDisc disc;          // its normal of any length
	bool pointValid = false;
	NiPoint3 point{0.0f, 0.0f, 0.0f};  // the contact point (game units)
	bool fromValid = false;
	NiPoint3 from{0.0f, 0.0f, 0.0f};   // the projectile's position at the callback: its last
	// The shield's outline (ShieldOutlineLogic.h) and the disc's in-plane
	// axes it is laid along, world; no outline: the circle of disc.radius.
	const ShieldOutline* outline = nullptr;
	NiPoint3 axisA{1.0f, 0.0f, 0.0f};
	NiPoint3 axisB{0.0f, 1.0f, 0.0f};
};

struct ArrowShieldVerdict {
	ArrowAtShield answer = ArrowAtShield::Vanilla;
	const char* why = "";
	float radialUnits = -1.0f;  // the point off the disc's middle, in its plane; -1 not read
	float alongUnits = -1.0f;   // the point off its plane; -1 not read
	bool bySegment = false;     // decided by the segment, not the point
	bool byOutline = false;     // the outline decided, not the circle
	float outlineUnits = 0.0f;  // outside the outline by this much (negative: inside)
};

inline bool FinitePoint(const NiPoint3& p) {
	return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

// A point's place against the outline: its reach along the disc's two
// in-plane axes from the disc's middle (the axes of any length).
inline float OutlineDistanceAt(const ArrowShieldInput& in, const NiPoint3& centre, const NiPoint3& p) {
	const NiPoint3 off = p - centre;
	const float la = std::sqrt(in.axisA.LengthSquared());
	const float lb = std::sqrt(in.axisB.LengthSquared());
	if (!(la > 0.0f) || !(lb > 0.0f)) {
		return 1e9f;
	}
	return OutlineDistance(*in.outline, DotOf3(off, in.axisA) / la, DotOf3(off, in.axisB) / lb);
}

// The verdict by the shield's outline, `d` the disc with a unit normal and
// `v` holding the point's distances: the point within the outline by the
// arrow's half width (kArrowHalfWidthUnits) and within the face's margin
// along the normal (kArrowDiscFaceMarginUnits, which is about where the
// engine puts the contact point, not about the shield) - blocked; else
// where the flight from its last position to the point crosses the disc's
// plane, by the same half width; else through.
inline ArrowShieldVerdict ArrowByOutline(const ArrowShieldInput& in, const BladeDisc& d, ArrowShieldVerdict v) {
	v.byOutline = true;
	v.outlineUnits = OutlineDistanceAt(in, d.centre, in.point);
	if (v.outlineUnits <= kArrowHalfWidthUnits && v.alongUnits <= d.halfThick + kArrowDiscFaceMarginUnits) {
		v.answer = ArrowAtShield::Blocked;
		v.why = "its point on the shield";
		return v;
	}
	v.answer = ArrowAtShield::Through;
	v.why = "its point off the shield";
	if (!in.fromValid || !FinitePoint(in.from) ||
	    (in.point - in.from).LengthSquared() < kArrowSegmentMinUnits * kArrowSegmentMinUnits) {
		return v;
	}
	v.bySegment = true;
	const float sa = DotOf3(in.from - d.centre, d.normal);
	const float sb = DotOf3(in.point - d.centre, d.normal);
	const float slab = d.halfThick > 0.0f ? d.halfThick : 0.0f;
	const bool crosses = (sa > 0.0f) != (sb > 0.0f) || (sa <= slab && sa >= -slab) || (sb <= slab && sb >= -slab);
	if (!crosses) {
		v.why = "its point off the shield, its flight short of its plane";
		return v;
	}
	const float t = sa != sb ? sa / (sa - sb) : 0.0f;
	const float clamped = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	v.outlineUnits = OutlineDistanceAt(in, d.centre, in.from + (in.point - in.from) * clamped);
	if (v.outlineUnits <= kArrowHalfWidthUnits) {
		v.answer = ArrowAtShield::Blocked;
		v.why = "its flight crossed the shield";
		return v;
	}
	v.why = "its flight crossed the plane off the shield";
	return v;
}

inline ArrowShieldVerdict ArrowAtShieldFor(const ArrowShieldInput& in) {
	ArrowShieldVerdict v;
	if (!in.targetIsPlayer || !in.byContact) {
		v.why = !in.targetIsPlayer ? "not the player" : "the shield not by contact";
		return v;
	}
	const float n2 = in.disc.normal.LengthSquared();
	const bool disc = in.discValid && in.disc.radius > 0.0f && n2 > 0.0f && std::isfinite(n2) &&
	                  FinitePoint(in.disc.centre);
	const bool point = in.pointValid && FinitePoint(in.point);
	if (!disc || !point) {
		v.answer = in.raised ? ArrowAtShield::AsRaised : ArrowAtShield::Through;
		v.why = !disc ? "the disc not placed" : "no usable point";
		return v;
	}
	BladeDisc d = in.disc;
	d.normal = d.normal * (1.0f / std::sqrt(n2));
	const NiPoint3 off = in.point - d.centre;
	const float along = DotOf3(off, d.normal);
	const NiPoint3 radial = off - d.normal * along;
	v.radialUnits = std::sqrt(radial.LengthSquared());
	v.alongUnits = along < 0.0f ? -along : along;
	if (in.outline != nullptr && in.outline->valid) {
		return ArrowByOutline(in, d, v);
	}
	// No outline read: the circle of the long spoke, as built first.
	if (v.radialUnits <= d.radius + kArrowDiscRimMarginUnits &&
	    v.alongUnits <= d.halfThick + kArrowDiscFaceMarginUnits) {
		v.answer = ArrowAtShield::Blocked;
		v.why = "its point on the disc";
		return v;
	}
	if (in.fromValid && FinitePoint(in.from) &&
	    (in.point - in.from).LengthSquared() >= kArrowSegmentMinUnits * kArrowSegmentMinUnits) {
		BladeDisc grown = d;
		grown.radius += kArrowSegmentMarginUnits;
		grown.halfThick = (d.halfThick > 0.0f ? d.halfThick : 0.0f) + kArrowSegmentMarginUnits;
		float fraction = 0.0f;
		NiPoint3 face{0.0f, 0.0f, 0.0f};
		bool inside = false;
		v.bySegment = true;
		if (SegmentEntersDisc(in.from, in.point, grown, fraction, face, inside)) {
			v.answer = ArrowAtShield::Blocked;
			v.why = "its flight crossed the disc";
			return v;
		}
		v.answer = ArrowAtShield::Through;
		v.why = "its point and its flight beside the disc";
		return v;
	}
	v.answer = ArrowAtShield::Through;
	v.why = "its point beside the disc";
	return v;
}

// The verdicts kept for the projectiles until the hit handler takes them,
// by the projectile's reference: another arrow's or a melee blow's attacker
// never matches. An entry the hit handler never asks for runs out.
inline constexpr UInt32 kArrowVerdictsMax = 8;
inline constexpr UInt32 kArrowVerdictFrames = 90;  // about a second; the damage follows within frames [G]

struct ArrowVerdicts {
	UInt32 count = 0;
	UInt32 projectile[kArrowVerdictsMax] = {};
	bool blocked[kArrowVerdictsMax] = {};
	UInt32 frame[kArrowVerdictsMax] = {};

	// Kept (or renewed) for this projectile; the oldest gives way when full.
	void Add(UInt32 p, bool isBlocked, UInt32 now) {
		if (p == 0) {
			return;
		}
		UInt32 at = count;
		for (UInt32 i = 0; i < count; ++i) {
			if (projectile[i] == p) {
				at = i;
			}
		}
		if (at == count) {
			if (count < kArrowVerdictsMax) {
				++count;
			} else {
				at = 0;
				for (UInt32 i = 1; i < count; ++i) {
					at = frame[i] < frame[at] ? i : at;
				}
			}
		}
		projectile[at] = p;
		blocked[at] = isBlocked;
		frame[at] = now;
	}

	// The verdict for this projectile, taken once.
	bool Take(UInt32 p, bool& isBlocked) {
		for (UInt32 i = 0; i < count; ++i) {
			if (projectile[i] == p && p != 0) {
				isBlocked = blocked[i];
				--count;
				projectile[i] = projectile[count];
				blocked[i] = blocked[count];
				frame[i] = frame[count];
				return true;
			}
		}
		return false;
	}

	// Drops what is older than kArrowVerdictFrames; answers how many.
	UInt32 Expire(UInt32 now) {
		UInt32 kept = 0;
		for (UInt32 i = 0; i < count; ++i) {
			if (now - frame[i] <= kArrowVerdictFrames) {
				projectile[kept] = projectile[i];
				blocked[kept] = blocked[i];
				frame[kept] = frame[i];
				++kept;
			}
		}
		const UInt32 dropped = count - kept;
		count = kept;
		return dropped;
	}
};

// The stub at 0x0060AD01 (replacing `call 005E5670`; ecx = esi = the target,
// ebx = the projectile, [ebp+8] = the contact point):
//
//   push ecx; push dword [ebp+8]; push ebx; push ecx
//   call callback                  ; cdecl ArrowAtShield(target, projectile, point)
//   add esp,12; pop ecx
//   test eax,eax; jnz decided
//   jmp 005E5670                   ; Vanilla: its ret goes back to 0x0060AD06
// decided:
//   cmp eax,3; jne branch
//   mov al,1; ret                  ; AsRaised: back to 0x0060AD06 with Block's yes
// branch:
//   add esp,4                      ; the return address to 0x0060AD06 dropped
//   mov edi,[ebp+8]
//   cmp eax,1; jne body
//   push 0x0060AD80; ret           ; Blocked: `mov al,1; jmp 0060AD8B`
// body:
//   push 0x0060AD84; ret           ; Through: `mov edi,[ebp+8]; mov al,[esp+1Bh]` (0)
inline constexpr UInt32 kArrowBlockCheckCall = 0x0060AD01;
inline constexpr UInt8 kArrowBlockCheckCallBytes[5] = {0xE8, 0x6A, 0xA9, 0xFD, 0xFF};  // call 005E5670
inline constexpr UInt32 kArrowShieldBranch = 0x0060AD80;
inline constexpr UInt32 kArrowBodyBranch = 0x0060AD84;
inline constexpr UInt32 kArrowShieldStubSize = 55;

inline void PutRel32(UInt8* out, UInt32 at, UInt32 stubBase, UInt32 target) {
	const UInt32 rel = target - (stubBase + at + 4);
	out[at] = static_cast<UInt8>(rel & 0xFF);
	out[at + 1] = static_cast<UInt8>((rel >> 8) & 0xFF);
	out[at + 2] = static_cast<UInt8>((rel >> 16) & 0xFF);
	out[at + 3] = static_cast<UInt8>((rel >> 24) & 0xFF);
}

inline void PutImm32(UInt8* out, UInt32 at, UInt32 value) {
	out[at] = static_cast<UInt8>(value & 0xFF);
	out[at + 1] = static_cast<UInt8>((value >> 8) & 0xFF);
	out[at + 2] = static_cast<UInt8>((value >> 16) & 0xFF);
	out[at + 3] = static_cast<UInt8>((value >> 24) & 0xFF);
}

// The stub's bytes for a stub placed at `stubBase`; answers the bytes
// written, 0 without room.
inline UInt32 BuildArrowShieldStub(UInt8* out, UInt32 capacity, UInt32 stubBase, UInt32 callback,
                                   UInt32 blockCheck) {
	if (out == nullptr || capacity < kArrowShieldStubSize) {
		return 0;
	}
	const UInt8 code[kArrowShieldStubSize] = {
		0x51,                    // 0  push ecx
		0xFF, 0x75, 0x08,        // 1  push dword [ebp+8]
		0x53,                    // 4  push ebx
		0x51,                    // 5  push ecx
		0xE8, 0, 0, 0, 0,        // 6  call callback
		0x83, 0xC4, 0x0C,        // 11 add esp,12
		0x59,                    // 14 pop ecx
		0x85, 0xC0,              // 15 test eax,eax
		0x75, 0x05,              // 17 jnz 24
		0xE9, 0, 0, 0, 0,        // 19 jmp blockCheck
		0x83, 0xF8, 0x03,        // 24 cmp eax,3
		0x75, 0x03,              // 27 jne 32
		0xB0, 0x01,              // 29 mov al,1
		0xC3,                    // 31 ret
		0x83, 0xC4, 0x04,        // 32 add esp,4
		0x8B, 0x7D, 0x08,        // 35 mov edi,[ebp+8]
		0x83, 0xF8, 0x01,        // 38 cmp eax,1
		0x75, 0x06,              // 41 jne 49
		0x68, 0, 0, 0, 0,        // 43 push kArrowShieldBranch
		0xC3,                    // 48 ret
		0x68, 0, 0, 0, 0,        // 49 push kArrowBodyBranch
		0xC3,                    // 54 ret
	};
	for (UInt32 i = 0; i < kArrowShieldStubSize; ++i) {
		out[i] = code[i];
	}
	PutRel32(out, 7, stubBase, callback);
	PutRel32(out, 20, stubBase, blockCheck);
	PutImm32(out, 44, kArrowShieldBranch);
	PutImm32(out, 50, kArrowBodyBranch);
	return kArrowShieldStubSize;
}

}  // namespace obvr::game
