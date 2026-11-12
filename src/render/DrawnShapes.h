#pragma once

#include "core/Types.h"

namespace obvr::render {

// Which shapes the renderer drew, for the worn-gear check ([Debug] WearCheck,
// game/WearCheck.h) - harness only: nothing is installed without the switch.
//
// Every NiTriShape and NiTriStrips is drawn through the renderer's
// Do_RenderShape 0x007693E0, which slots 88 and 89 of the NiDX9Renderer
// vtable 0x00A88EA4 both hold (read in Oblivion.exe 1.2.0.416, 2026-11-12;
// OBNG re/ledger.md: the geometry's slot 33 calls it through these slots).
// The two slots are pointed at a recorder that notes the geometry and calls
// the function the slot held - an entry detour another plugin keeps there
// (OBNG's occlusion cull) still runs. Installed only after both slots read
// 0x007693E0; anything else there and nothing is written.
//
// The record runs between Begin and End. Draws inside the world render's
// bracket count as the eyes' draws - unless a cull of another camera or the
// water's reflection is under way (the actor shadow maps and the reflection
// draw the same shapes: render/CullingHook.cpp holds the record for those);
// draws outside the bracket count apart. The draws under a held cull are kept
// apart as well: the menus' 2D layer is drawn inside the world render's
// bracket (betweenPasses and afterSecondPass, render/SceneRenderHook.cpp),
// and its paperdoll through a cull of the menu's own camera - so the
// paperdoll's shapes, which nothing else draws, are found there
// (finery-checks 2026-11-12 21:53: 151 held draws with the inventory open,
// 25 without).
bool InstallDrawnShapes();

void BeginDrawnShapes();
void EndDrawnShapes();
bool DrawnShapesRecording();

// The world render's bracket (game/WearCheck.cpp, from the camera hook's
// before- and after-world-render steps).
void SetDrawnShapesWorld(bool inside);

// A cull inside the world render that is not the eyes' own: another camera
// than the scene graph's (the actor shadow maps, 0x0070E152's ten passes a
// frame - OBNG knowledge/culling-and-draw-setup.md) or the water's culling
// process (vtable 0x00A3F380, OBNG's driver probe 2026-11-12). Held while
// such a cull runs: its draws are noted nowhere.
bool IsEyeCull(const void* process, const void* camera);

// For render/CullingHook.cpp: holds the record through one cull that is not
// the eyes' own, while a record runs; nothing otherwise.
class DrawnShapesHold {
public:
	DrawnShapesHold(const void* process, const void* camera);
	~DrawnShapesHold();
	DrawnShapesHold(const DrawnShapesHold&) = delete;
	DrawnShapesHold& operator=(const DrawnShapesHold&) = delete;

private:
	bool m_held = false;
};

bool DrawnInWorld(const void* geometry);
bool DrawnInOtherCull(const void* geometry);
// The menu's figure ("PlayerInventoryPC") found above a shape drawn outside
// the eyes' culls while the record ran, and how many of its shapes' draws
// were seen; null and 0 when the menus drew none.
const void* DrawnPaperdoll();
// Called once a record with the figure, at the first of its shapes' draws -
// while it hangs in the menu's scene, where its tree and its bones can be
// read (the model is not kept to the next frame).
void SetDrawnPaperdollSeen(void (*seen)(const void* model));
UInt32 DrawnPaperdollDraws();
bool DrawnElsewhere(const void* geometry);
UInt32 DrawnWorldCount();
UInt32 DrawnElsewhereCount();
UInt32 DrawnHeldCount();  // draws not noted because a hold was on

}  // namespace obvr::render
