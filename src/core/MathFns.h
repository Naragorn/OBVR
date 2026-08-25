#pragma once

// Die wenigen Gleitkommafunktionen, die OBVR braucht.
//
// Eigener Header statt <cmath> in jeder Datei: im freestanding Cross-Build
// gibt es keine Standardbibliothek, die Funktionen kommen dort direkt aus
// msvcrt. Nativ gebaute Tests nehmen den normalen Weg.
//
// Bewusst ohne Windows-Header, damit reine Rechendateien auch auf einem
// Linux-Host uebersetzen.

#if defined(OBVR_NO_WINSDK)

// Bewusst die double-Varianten: sinf und cosf fehlen in aelteren
// msvcrt-Staenden, sin und cos sind ueberall vorhanden.
extern "C" double __cdecl sin(double value);
extern "C" double __cdecl cos(double value);
extern "C" double __cdecl sqrt(double value);

#else

#include <cmath>

#endif

namespace obvr::math {

constexpr float kDegreesToRadians = 0.01745329252f;

inline float Sin(float radians) { return static_cast<float>(sin(static_cast<double>(radians))); }
inline float Cos(float radians) { return static_cast<float>(cos(static_cast<double>(radians))); }
inline float Sqrt(float value) { return static_cast<float>(sqrt(static_cast<double>(value))); }

}  // namespace obvr::math
