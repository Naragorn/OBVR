#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The soft push: the hands press the visible body's breasts in, and they
// spring back when the hands leave (Nadi, 2026-10-28: "haben unsere hände
// eig kollision ... der hat ja bbb").
//
// Oblivion's BBB is no physics: the body mesh is weighted to extra bones
// that animations move, nothing collides with them. The installed upper
// body (Data/Meshes/Characters/_male/femaleupperbody.nif, read 2026-10-28)
// weights to "Bip01 L/R Op1".."Op3" - the BBB breast chain (named so on
// the Japanese modding side, "oppai"; NifSkope shows them on every BBB
// outfit). The push moves each chain's root (Op1) in its parent's frame, so
// the chain goes with it, and measures at Op2, the next bone out. A body
// without the bones (a male body, a body replacer without BBB) is left
// alone.
//
// Everything here is plain values; game/BodyIk.cpp books the writes.

inline constexpr UInt32 kSoftPushSides = 2;
inline const char* const kSoftPushBoneNames[kSoftPushSides] = {"Bip01 R Op1", "Bip01 L Op1"};
inline const char* const kSoftPushTipNames[kSoftPushSides] = {"Bip01 R Op2", "Bip01 L Op2"};

namespace soft_push_detail {
inline bool Finite(float v) { return v == v && v - v == 0.0f; }
inline bool Finite(const NiPoint3& p) { return Finite(p.x) && Finite(p.y) && Finite(p.z); }
}  // namespace soft_push_detail

// How far `point` is pushed by the hands, world units: each hand nearer
// than `reach` pushes it straight away from itself by what it is inside
// the reach; the pushes add, and the sum is no longer than `most`. A hand
// exactly on the point gives no direction and pushes nothing; a hand not
// read, or not a number, is left out.
inline NiPoint3 SoftPushWanted(const NiPoint3& point, const NiPoint3* hands, const bool* valid, UInt32 count,
                               float reach, float most) {
	using soft_push_detail::Finite;
	NiPoint3 sum{0.0f, 0.0f, 0.0f};
	if (hands == nullptr || valid == nullptr || !Finite(point) || !Finite(reach) || !Finite(most) || !(reach > 0.0f) ||
	    !(most > 0.0f)) {
		return sum;
	}
	for (UInt32 i = 0; i < count; ++i) {
		if (!valid[i] || !Finite(hands[i])) {
			continue;
		}
		const NiPoint3 away = point - hands[i];
		const float distance = math::Sqrt(away.LengthSquared());
		if (!(distance > 1.0e-3f) || !(distance < reach)) {
			continue;
		}
		sum = sum + away * ((reach - distance) / distance);
	}
	const float length = math::Sqrt(sum.LengthSquared());
	if (length > most) {
		sum = sum * (most / length);
	}
	return sum;
}

// This frame's offset from last frame's: pressed as far or further, the
// hand drives it at once (it never lags a hand into the body); otherwise
// it eases towards what the hands want, `returnPerSecond` of the way a
// second, never past. No time, or a time that is not a number, keeps it.
inline NiPoint3 StepSoftPushOffset(const NiPoint3& current, const NiPoint3& wanted, float dtSeconds,
                                   float returnPerSecond) {
	using soft_push_detail::Finite;
	if (!Finite(wanted)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	if (!Finite(current) || wanted.LengthSquared() >= current.LengthSquared()) {
		return wanted;
	}
	if (!Finite(dtSeconds) || !(dtSeconds > 0.0f) || !Finite(returnPerSecond)) {
		return current;
	}
	float share = dtSeconds * returnPerSecond;
	share = share < 1.0f ? share : 1.0f;
	share = share > 0.0f ? share : 0.0f;
	return current + (wanted - current) * share;
}

// A world offset in the frame of a parent turned by `parentRot` and scaled
// by `parentScale`: what to add to a child's local position to move it by
// `world`. A parent with no scale, or one that is not a number, takes none.
inline NiPoint3 WorldOffsetToLocal(const NiMatrix33& parentRot, float parentScale, const NiPoint3& world) {
	if (!soft_push_detail::Finite(parentScale) || !(parentScale > 1.0e-6f)) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	return InverseMatrix(parentRot) * world * (1.0f / parentScale);
}

}  // namespace obvr::game
