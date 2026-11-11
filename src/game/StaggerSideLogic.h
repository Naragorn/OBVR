#pragma once

#include "core/MathFns.h"
#include "core/Types.h"

namespace obvr::game {

// The side a stagger falls to ([Hands] StaggerSides; Nadi, 2026-11-05: "wenn
// ich ihn nach rechts schlage erwartet ich dass er nach rechts taumelt. er
// geht aber nach links").
//
// Read (OBNG knowledge/hit-reactions.md, "Which stagger animation plays"):
// the stagger start 0x005F4FD0 takes no direction, and vanilla has one file
// per stance. The engine keeps every file of a group in an
// AnimSequenceMultiple and draws one at random (0x00470BF0, index 0xFF), or
// takes the index it is given. So a mirrored copy of each stance's file
// (tools/kf-mirror.py, in SpecialAnims) joins the group, and for a blow of
// the player's OBVR hands the index of the file that leans the blow's way.
//
// Which way each vanilla file leans was computed from the files themselves
// (the B-spline tracks evaluated on the vanilla skeleton, 2026-11-05): the
// head moves 11 to 17 units to the actor's right (+x) in the one-handed,
// two-handed and staff staggers, and about 10 units to its left in the
// fists'. The mirror leans the other way (its head's x is the negative).

inline constexpr char kStaggerMirrorTag[] = "_obvrmirror";

// The names OBVR adds to an NPC's special animations (TESAnimation). The
// engine reads a special animation's group and stance from the part of the
// name before the first underscore, as Blockhead's overrides rely on.
inline constexpr const char* kStaggerMirrorFiles[] = {
	"OneHandStagger_OBVRMirror.kf",
	"TwoHandStagger_OBVRMirror.kf",
	"HandToHandStagger_OBVRMirror.kf",
	"StaffStagger_OBVRMirror.kf",
};
inline constexpr UInt32 kStaggerMirrorFileCount = sizeof(kStaggerMirrorFiles) / sizeof(kStaggerMirrorFiles[0]);

// How far sideways the swing has to have gone (metres, in the world, along
// the actor's right) before its side decides; less leaves the engine's draw.
inline constexpr float kStaggerBlowLateralMin = 0.05f;

inline char StaggerLowerAscii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

inline bool SameAnimName(const char* a, const char* b) {
	if (a == nullptr || b == nullptr) {
		return false;
	}
	for (; *a != 0 && *b != 0; ++a, ++b) {
		if (StaggerLowerAscii(*a) != StaggerLowerAscii(*b)) {
			return false;
		}
	}
	return *a == 0 && *b == 0;
}

// Whether `text` begins with `prefix` (lower case), case aside.
inline bool StaggerStartsNoCase(const char* text, const char* prefix) {
	for (; *prefix != 0; ++text, ++prefix) {
		if (*text == 0 || StaggerLowerAscii(*text) != *prefix) {
			return false;
		}
	}
	return true;
}

inline bool StaggerContainsNoCase(const char* text, const char* part) {
	if (text == nullptr) {
		return false;
	}
	for (; *text != 0; ++text) {
		if (StaggerStartsNoCase(text, part)) {
			return true;
		}
	}
	return false;
}

inline bool IsStaggerMirror(const char* path) { return StaggerContainsNoCase(path, kStaggerMirrorTag); }

// The file's own name, past its folders.
inline const char* AnimFileStem(const char* path) {
	const char* stem = path;
	for (const char* p = path; *p != 0; ++p) {
		if (*p == '\\' || *p == '/') {
			stem = p + 1;
		}
	}
	return stem;
}

// The side a stagger file leans to: +1 the actor's right, -1 its left, 0 no
// stagger of a known side.
inline int StaggerFileLean(const char* path) {
	if (path == nullptr) {
		return 0;
	}
	const char* stem = AnimFileStem(path);
	int lean = 0;
	if (StaggerStartsNoCase(stem, "handtohandstagger")) {
		lean = -1;
	} else if (StaggerStartsNoCase(stem, "onehandstagger") || StaggerStartsNoCase(stem, "twohandstagger") ||
	           StaggerStartsNoCase(stem, "staffstagger")) {
		lean = 1;
	}
	return IsStaggerMirror(stem) ? -lean : lean;
}

// A world vector's part along the actor's right, the actor turned by rotZ
// (TESObjectREFR +0x28; its right is (cos z, -sin z), as RoomFollowLogic.h
// WorldStepToActor has it).
inline float BlowLateral(float worldX, float worldY, float rotZ) {
	return math::Cos(rotZ) * worldX - math::Sin(rotZ) * worldY;
}

// The turn from heading `from` to heading `to` (radians), as degrees the
// short way round, -180..180 (the stagger's watch, Nadi 2026-11-11: mirrored
// staggers turned the staggered one round).
inline float HeadingTurnDegrees(float from, float to) {
	float d = (to - from) * 57.29578f;
	while (d > 180.0f) {
		d -= 360.0f;
	}
	while (d < -180.0f) {
		d += 360.0f;
	}
	return d;
}

// How far the actor turned by rotZ looks away from a point (dx, dy) off it,
// degrees, 0 facing it: its forward is (sin z, cos z).
inline float FacingOffDegrees(float rotZ, float dx, float dy) {
	if (dx == 0.0f && dy == 0.0f) {
		return 0.0f;
	}
	return HeadingTurnDegrees(rotZ, math::Atan2(dx, dy));
}

// Where the body faces against the actor's heading, from its shoulders: the
// left one at (lx, ly), the right at (rx, ry) in the world; the body's right
// runs left to right and its front is that turned a quarter (the heading's
// right (cos z, -sin z) has the front (sin z, cos z)). 0: the body faces as
// the reference does, 180: turned round.
inline float BodyFacingOffDegrees(float rotZ, float lx, float ly, float rx, float ry) {
	const float x = rx - lx;
	const float y = ry - ly;
	if (x == 0.0f && y == 0.0f) {
		return 0.0f;
	}
	return HeadingTurnDegrees(rotZ, math::Atan2(-y, x));
}

// The index of the group's file to play for a blow `lateral` to the actor's
// right (negative: its left), or -1 to leave the engine's random draw: the
// blow too little sideways (or not a number), or no file leaning its way.
inline int PickStaggerEntry(const char* const* names, int count, float lateral, float minLateral) {
	if (names == nullptr || !(lateral >= minLateral || lateral <= -minLateral)) {
		return -1;
	}
	const int want = lateral > 0.0f ? 1 : -1;
	for (int i = 0; i < count; ++i) {
		if (names[i] != nullptr && StaggerFileLean(names[i]) == want) {
			return i;
		}
	}
	return -1;
}

// Whether an NPC with this skeleton gets the mirrors: they were made for the
// vanilla skeleton, and the engine looks for special animations in the
// skeleton's own folder (Characters\_Male\SpecialAnims).
inline bool SkeletonTakesStaggerMirrors(const char* modelPath) {
	if (modelPath == nullptr || !StaggerStartsNoCase(modelPath, "characters")) {
		return false;
	}
	const char* rest = modelPath + 10;
	if (*rest != '\\' && *rest != '/') {
		return false;
	}
	++rest;
	if (!StaggerStartsNoCase(rest, "_male")) {
		return false;
	}
	rest += 5;
	if (*rest != '\\' && *rest != '/') {
		return false;
	}
	++rest;
	for (; *rest != 0; ++rest) {
		if (*rest == '\\' || *rest == '/') {
			return false;  // a folder below _Male: not the skeleton's own
		}
	}
	return true;
}

// The pick reaches the engine at 0x0047628A, in 0x00476260 (the sequence for
// a group code): `mov eax, [edx+10h]; push -1; call eax` (8B 42 10 6A FF FF
// D0) - the group's entry, AnimSequenceBase vtable +0x10, thiscall(entry,
// SInt8 index) ret 4, asked for index -1, the random draw. Those seven bytes
// become `call stub; nop; nop`, and the stub asks OBVR's pick (cdecl,
// int(void* entry)) for the index and makes the same call with it:
//
//   push ecx                 ; the entry, kept
//   push ecx                 ; the pick's argument
//   call pick
//   add  esp, 4
//   pop  ecx
//   mov  edx, [ecx]          ; its vtable again (the pick may change edx)
//   push eax                 ; the index: -1 the draw, as before
//   call [edx+10h]
//   ret                      ; back to the two nops, then 0x00476291
inline constexpr UInt32 kStaggerPickSite = 0x0047628A;
inline constexpr UInt8 kStaggerPickSiteBytes[] = {0x8B, 0x42, 0x10, 0x6A, 0xFF, 0xFF, 0xD0};
inline constexpr UInt32 kStaggerPickStubLength = 18;

inline void StaggerPutRel32(UInt8* at, UInt32 from, UInt32 to) {
	const UInt32 rel = to - (from + 4);
	at[0] = static_cast<UInt8>(rel);
	at[1] = static_cast<UInt8>(rel >> 8);
	at[2] = static_cast<UInt8>(rel >> 16);
	at[3] = static_cast<UInt8>(rel >> 24);
}

// The stub, to run at stubAddress. Answers its length, or 0 when the
// capacity is too small.
inline UInt32 BuildStaggerPickStub(UInt8* buffer, UInt32 capacity, UInt32 stubAddress, UInt32 pick) {
	if (buffer == nullptr || capacity < kStaggerPickStubLength) {
		return 0;
	}
	UInt8* b = buffer;
	*b++ = 0x51;
	*b++ = 0x51;
	*b++ = 0xE8;
	StaggerPutRel32(b, stubAddress + 3, pick);
	b += 4;
	*b++ = 0x83;
	*b++ = 0xC4;
	*b++ = 0x04;
	*b++ = 0x59;
	*b++ = 0x8B;
	*b++ = 0x11;
	*b++ = 0x50;
	*b++ = 0xFF;
	*b++ = 0x52;
	*b++ = 0x10;
	*b++ = 0xC3;
	return static_cast<UInt32>(b - buffer);
}

// The seven bytes at the site: the call to the stub and two nops.
inline UInt32 BuildStaggerPickSitePatch(UInt8* buffer, UInt32 capacity, UInt32 stubAddress) {
	if (buffer == nullptr || capacity < sizeof(kStaggerPickSiteBytes)) {
		return 0;
	}
	buffer[0] = 0xE8;
	StaggerPutRel32(buffer + 1, kStaggerPickSite + 1, stubAddress);
	buffer[5] = 0x90;
	buffer[6] = 0x90;
	return sizeof(kStaggerPickSiteBytes);
}

}  // namespace obvr::game
