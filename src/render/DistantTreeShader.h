#pragma once

namespace obvr::render {

// The distant trees' vertex shader swapped for the one faced by the trees'
// place (render/DistantTreeFacing.h) as the game sets it: each shader the
// game sets is looked at once; the one that turns its instances by EyeDir
// gets a patched copy, which is set in its place while [Render]
// StableDistantTrees is on. Called from the device's SetVertexShader hook.
void* SelectDistantTreeShader(void* device, void* shader, bool enabled);

}  // namespace obvr::render
