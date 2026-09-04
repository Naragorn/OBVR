#pragma once

#include "core/Types.h"

namespace obvr::game {

extern const UInt8 kWorldPickOriginalBytes[7];
extern const UInt8 kWorldPickHudInfoOriginalCall[5];

UInt32 BuildWorldPickTrampoline(UInt8* buffer, UInt32 capacity,
                                UInt32 trampolineAddress, UInt32 callbackAddress);
UInt32 BuildWorldPickPatch(UInt8* buffer, UInt32 capacity, UInt32 hookAddress,
                           UInt32 trampolineAddress);
UInt32 BuildWorldPickHudInfoCallPatch(UInt8* buffer, UInt32 capacity,
                                      UInt32 callAddress, UInt32 replacementAddress);

}  // namespace obvr::game
