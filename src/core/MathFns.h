#pragma once

// The handful of floating point functions OBVR needs.
//
// A header of its own instead of <cmath> in every file: the freestanding
// cross build has no standard library, and the functions come straight from
// msvcrt there. Natively built tests take the normal route.
//
// Deliberately without Windows headers, so that files which only do
// arithmetic also compile on a Linux host.

#if defined(OBVR_NO_WINSDK)

// Deliberately the double variants: sinf and cosf are missing from older
// msvcrt revisions, while sin and cos are present everywhere.
extern "C" double __cdecl sin(double value);
extern "C" double __cdecl cos(double value);
extern "C" double __cdecl sqrt(double value);
extern "C" double __cdecl atan(double value);
extern "C" double __cdecl tan(double value);

#else

#include <cmath>

#endif

namespace obvr::math {

constexpr float kDegreesToRadians = 0.01745329252f;

inline float Sin(float radians) { return static_cast<float>(sin(static_cast<double>(radians))); }
inline float Cos(float radians) { return static_cast<float>(cos(static_cast<double>(radians))); }
inline float Sqrt(float value) { return static_cast<float>(sqrt(static_cast<double>(value))); }
inline float Tan(float radians) { return static_cast<float>(tan(static_cast<double>(radians))); }

// Only ever used to turn a projection tangent into a readable number of
// degrees for the log. Nothing depends on its precision, which is why the
// single-argument atan is enough and atan2 is not needed.
inline float Atan(float value) { return static_cast<float>(atan(static_cast<double>(value))); }

// How close to straight up or down Asin will answer for. The identity below
// divides by the cosine, which goes to zero at the poles, so the input is
// held just short of them: 0.9999 is 89.19 degrees, past anything a player
// can look at - Oblivion itself stops at 89 - and it leaves the divisor at
// 0.0141 rather than at nothing.
constexpr float kAsinLimit = 0.9999f;

// The inverse sine, out of the atan that is already here.
//
// Built rather than declared, and the reason is the same one that put the
// double variants above: this build links against whatever msvcrt the system
// has, and every name added to that list is another way for a machine to
// fail to start the game. asin(s) = atan(s / sqrt(1 - s*s)) is exact for
// every input this is given, and it is spelled with two functions the build
// already calls - so it costs no new dependency at all.
//
// The sine is what OBVR has, because that is what a rotation matrix holds
// directly (see SinPitchOf). The angle is what Oblivion's own rotation
// wants. This is the one conversion between them.
inline float Asin(float sine) {
	const float clamped = sine > kAsinLimit ? kAsinLimit : (sine < -kAsinLimit ? -kAsinLimit : sine);
	return Atan(clamped / Sqrt(1.0f - clamped * clamped));
}

constexpr float kRadiansToDegrees = 57.29577951f;

}  // namespace obvr::math
