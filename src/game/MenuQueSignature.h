#pragma once
#include "core/Types.h"

namespace obvr::game {
// MenuQue v16b Submodule.Game.dll's complete bool __cdecl poll(int*)
// function, RVA EA30. Its three absolute operands are relocated by Windows.
// No patch is made. Refuse every byte/operand mismatch before calling it.
inline bool MenuQuePollMatches(const UInt8* bytes, UInt32 base) {
 if (!bytes) return false;
 const UInt8 expected[] = {
  0x55,0x8B,0xEC,0x8B,0x45,0x08,0x8B,0x0D,0,0,0,0,
  0x89,0x08,0x80,0x3D,0,0,0,0,0,0x74,0x0B,
  0xC6,0x05,0,0,0,0,0,0xB0,1,0x5D,0xC3,0x32,0xC0,0x5D,0xC3
 };
 for (unsigned i=0; i<sizeof(expected); ++i) {
  UInt8 wanted=expected[i];
  if (i>=8 && i<12) wanted=static_cast<UInt8>((base+0x50ED8) >> ((i-8)*8));
  if (i>=16 && i<20) wanted=static_cast<UInt8>((base+0x50ED4) >> ((i-16)*8));
  if (i>=25 && i<29) wanted=static_cast<UInt8>((base+0x50ED4) >> ((i-25)*8));
  if (bytes[i]!=wanted) return false;
 }
 return true;
}
}
