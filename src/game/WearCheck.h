#pragma once

namespace obvr::game {

// [Debug] WearCheck: at each hand script's mark, whether the worn gear is
// drawn as it should be - judged by the rules of game/WearCheckLogic.h over
// the shapes the renderer drew (render/DrawnShapes.h), so a harness run says
// red or green without a picture being read:
//
//   * the hands: each side's drawn shapes skinned to its finger bones, one
//     with five fingers wanted ("hands ok"; "WEAR WRONG" for a hand doubled
//     or short of fingers);
//   * every worn item: its shapes over the body's tree and the first-person
//     tree, how many shown and how many the eyes' passes drew ("every shape
//     drawn");
//   * the menu's paperdoll: the same items over its own tree, drawn by the
//     menus' pass, and its head and feet inside the menu's view.
//
// The mark's frame is read before its world render (the hidden bits and the
// bones as OBVR left them for the draw) and its draws recorded from there;
// the paperdoll is read at its first draw (the engine hangs it in the menu's
// scene for the draw only); the judgement is logged at the next world
// render, when the frame - its 2D layer with the paperdoll included - is over.
//
// Called from the camera hook: before the first scene pass and after the
// world render.
void WearCheckBeforeWorld();
void WearCheckAfterWorld();

}  // namespace obvr::game
