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

#else

#include <cmath>

#endif

namespace obvr::math {

constexpr float kDegreesToRadians = 0.01745329252f;

inline float Sin(float radians) { return static_cast<float>(sin(static_cast<double>(radians))); }
inline float Cos(float radians) { return static_cast<float>(cos(static_cast<double>(radians))); }
inline float Sqrt(float value) { return static_cast<float>(sqrt(static_cast<double>(value))); }

// Only ever used to turn a projection tangent into a readable number of
// degrees for the log. Nothing depends on its precision, which is why the
// single-argument atan is enough and atan2 is not needed.
inline float Atan(float value) { return static_cast<float>(atan(static_cast<double>(value))); }

constexpr float kRadiansToDegrees = 57.29577951f;

}  // namespace obvr::math
