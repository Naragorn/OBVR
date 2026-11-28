#pragma once

namespace obvr::render {

// Detours the engine's actor shadow render (addr::kRenderActorShadows) so the
// second eye of the dual pass can keep the first eye's shadow maps
// (render/ShadowShare.h, [Render] ShareActorShadows). Pass-through while the
// setting is off. False when the entry bytes differ or memory cannot be
// written; the shadows are then drawn per eye as before. Install once.
bool InstallShadowShareHook();

// How many times the mirror's passes kept the frame's actor shadow maps
// instead of drawing them, since the start (ShadowShareState::mirrorKept): the
// mirror's cost line reads the difference over its window.
unsigned MirrorShadowMapsKept();

}  // namespace obvr::render
