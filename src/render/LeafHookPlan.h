#pragma once
#include "core/Types.h"

namespace obvr::render {
struct LeafHookSite { UInt32 address, original; };
inline constexpr LeafHookSite kLeafHookSites[] = {
    {0x007F16CF, 0x007F1170}, // initial geometry setup
    {0x007F8B51, 0x007F0BC0}, // next tree, different geometry data
    {0x007F8C66, 0x007F0BC0}, // next tree, shared geometry data
};

// Partial installation stays inert. Retry checks all remaining signatures
// before writing and leaves already owned sites alone.
template<class Verify, class Write>
bool InstallLeafSites(unsigned& installed, Verify verify, Write write) {
    for (unsigned i = 0; i < 3; ++i)
        if (!(installed & (1u << i)) && !verify(i)) return false;
    for (unsigned i = 0; i < 3; ++i) {
        if (installed & (1u << i)) continue;
        if (!write(i)) return false;
        installed |= 1u << i;
    }
    return installed == 7;
}
}
