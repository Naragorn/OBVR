// Only compiled as part of the SDK-free verification build.
//
// Without the CRT a few symbols are missing that the MSVC-compatible code
// generator assumes exist. In a regular MSVC build the runtime provides them
// itself, which is why this file must not be built there.

#if defined(OBVR_NO_WINSDK)

// The compiler emits a reference to _fltused as soon as floating point is
// used. The value is a historical marker of the MSVC runtime; only its
// existence matters.
extern "C" int _fltused = 0x9875;

#endif
