#pragma once
#include <cmath>
#include "game/NiMath.h"

namespace obvr::render {
// Cylindrical leaf cards face the common midpoint between the rendered eyes.
// Both eyes therefore get the same basis, while physically moving around a
// tree turns its front face toward the actual viewpoint. No camera yaw is used.
inline bool LeafFacing(bool enabled, bool world, bool mainCamera,
                       const NiPoint3& camera, const NiPoint3& tree,
                       NiPoint3& right, NiPoint3& up) {
    if (!enabled || !world || !mainCamera) return false;
    const float dx = tree.x - camera.x, dy = tree.y - camera.y;
    const float lengthSq = dx * dx + dy * dy;
    if (!(lengthSq > 0.0001f && lengthSq < 1.0e20f)) return false;
    const float inv = 1.0f / std::sqrt(lengthSq);
    right = {dy * inv, -dx * inv, 0};
    up = {0, 0, 1};
    return true;
}
}
