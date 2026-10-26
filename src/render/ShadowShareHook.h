#pragma once

namespace obvr::render {

// Detours the engine's actor shadow render (addr::kRenderActorShadows) so the
// second eye of the dual pass can keep the first eye's shadow maps
// (render/ShadowShare.h, [Render] ShareActorShadows). Pass-through while the
// setting is off. False when the entry bytes differ or memory cannot be
// written; the shadows are then drawn per eye as before. Install once.
bool InstallShadowShareHook();

}  // namespace obvr::render
