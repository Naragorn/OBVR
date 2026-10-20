# Left-handed play - everything that has to mirror

The tester, 2026-09-30: "links händer: erstelle eine spec die alles sammelt was gespiegelt werden muss für links händer ... und generell eine option in den settings wo was sein soll. denke da an 2 händer rechte hand rechte schulter zum ziehen bei mir als rechtshänder. links händer wollen whl alles links, kann aber auch n paar geben die wollen dann mixen und matchen. soll möglich sein."

Scope: Full VR (`[Hands] Enabled=1`). The inventory below was read from the code on 2026-09-30 (file:line at that commit). Nothing in it has been tried by a left-hander in a headset. **2026-10-20: the mirror is built (section 2.1, part of `LeftHanded` - no switch of its own, a right hand where the left one is serves nobody), with the palm, laser-yaw and rest-offset items of sections 2 and 3 and the body's arms reaching across**, after a tester's report of 2026-10-17 ("left handed mode inverts hands to face the wrong direction", controls-spec.md "Beta reports of 2026-10-17").

## 1. How left-handed works today

`[Hands] LeftHanded=1` swaps the two controllers **as a whole**, before anything reads them: pose, buttons, sticks, finger curls and velocities (`vr::AssignHandRoles`, src/vr/HandInput.h:1278, called at src/camera/CameraHook.cpp:1720).
- Everything downstream works by **role**: "right" is the weapon hand, "left" the other hand, whichever controller that is.
- What hangs on a physical device - the laser beam, the wrist quads, the controller models - maps the role back with `vr::HandDeviceForRole` (HandInput.h:1289).
- The swap only happens while Full VR is active (`active &&`, CameraHook.cpp:1721). With the controllers only steering menus nothing swaps.

The consequence that matters most: **the bodies are not mirrored.** The game's weapon hangs on its right hand bone ("Bip01 R Hand"), and a left-hander's left controller drives that bone. So a left-hander holds the weapon in a right hand, drawn where their left hand is (section 2).

## 2. The hands themselves

| What | Where | Left-handed today | Status |
| --- | --- | --- | --- |
| Hand bones | CameraHook.cpp (`PinAdjustableHand`) | "Bip01 R Hand" follows the left controller, "Bip01 L Hand" the right. With the mirror (2.1) the whole first-person skeleton is drawn turned over, so the right hand's mesh on the left controller is a left hand. | **Built 2026-10-20** (the mirror). |
| Hand calibration | CameraHook.cpp (`PinAdjustableHand`) | The right hand's calibration (`RightHandRoll/Pitch/Yaw`, `RightHandGrip*`) is used on the left controller - mirrored in the controller's frame with the mirror on (`game::MirroredCalibration`, `MirroredGrip`), which is exactly the left controller's fit, since the controllers are mirror images. A fit made with the mirror on is turned back before it is saved (`UnmirroredFit`), so the keys stay right-handed and one profile serves both handednesses. | **Built 2026-10-20.** The separate second set is no longer needed; only two hands that hold their controllers differently would want one. |
| Arms at rest | HandMode.cpp (`ArmsRestOffset`) | The rest offset `restHandRight=+0.20` is mirrored with the skeleton (the weapon hand rests at -0.20), so the root no longer slides 40 cm to the side. Without the mirror it is as before. | **Built 2026-10-20** (hand_mode_test). |
| Finger tracking | HandGrip.h:233, 242 | Curls come from the swapped controller. The left bone's mirror (`MirroredForLeft`) goes by bone. | OK. |
| Controller models | CameraHook.cpp:4658-4667 | Drawn at each physical controller. | OK. |

### 2.1 A left hand holding the weapon - the mirror, built 2026-10-20

For a left-hander to see a **left** hand holding the weapon, one of these had to happen:
- **The weapon on the game's left hand bone.** Re-parent the "Weapon" node under "Bip01 L Hand" and pin the bones by physical side again. Every weapon's attach point is modelled for a right hand, so each weapon would need a mirrored local transform, and so would the shield on "Bip01 L ForearmTwist", the torch, the magic node, the bow and the quiver; every lookup by node name in OBVR (the two-hand pins, the blade contact, the bow by hand, the body IK's hides) would need its mirror twin. Large, not tried.
- **A mirrored first-person skeleton.** The route built. Not a scale of -1 (an NiTransform's scale is one float for all three axes) but a rotation turned over: the root's local rotation gets `MirrorX()` (x to -x) between the hands' turn and the engine's base (`game::MirroredRootRotation`, FirstPersonArmsLogic.h), in the parent's frame - the camera's on foot - so the whole skeleton is its own mirror image about the head's midplane: the right arm hangs on the left, the left on the right, the weapon, shield, torch and magic node with them, every engine lookup untouched. The hands are pinned to the real controllers with the calibration and the grip mirrored in the controller's frame (`game::MirroredCalibration`, `MirroredGrip`, BonePin.h "The mirror"): the wanted world rotation has determinant -1 and the bone's local under its turned-over parent comes out proper again, so the engine sees ordinary locals everywhere but at the root. A mirrored mesh winds its triangles the other way, so the renderer swaps the cull for every draw of the engine's first-person pass while the mirror is on (`render::MirroredCullFor`, `SwapCullForMirror` in InterfaceRenderHook.cpp, the pass known from the accumulator byte the closed-hands pass already reads); the closed hands' inside draw after it reads the swapped cull and draws the engine's own, the inside of the mirrored mesh. Lighting needs nothing: a mirrored surface's normal is the mirrored normal.
- **Live with it.** The weapon hand is a right hand on the left controller - the picture until 2026-10-20. It had a switch for an afternoon (`LeftHandMirror`); it went the same day, since a right hand where the left one is serves nobody, and a mirrored draw that renders wrong in a headset is a bug to fix in the cull code, not a picture to switch back to.

**The body's arms under the mirror (built 2026-10-20, the same day):** left-handed, the body's arms used to be left to the body (hidden with `[Body] HideArms`) rather than reach across the chest. Now each body arm reaches the first-person hand of the other side - the body's right arm the "Bip01 L Hand" bone, pinned to the right controller - and that side's first-person arm is laid onto it (`game::FirstPersonSideFor`, BodyIk.cpp `SolveArm` and `AlignFirstPersonArm`). The two skeletons' left and right bones are not plain mirror images of each other (a Bip01 left bone's axes are the right's turned about x, HandMode.h `leftHandRoll`), so the conversion is not guessed but read from the body's own skins: NiSkinData's skin-to-bone of each bone is the inverse of its bind pose in the skin's space, where the bind pose is symmetric about x, so the partner `skinToBone_from * S * inv(skinToBone_to)` carries one side's bone frame to the other's twin exactly (game/BoneMirror.h, `MirrorPartner`, `PartnerWorld`; read once per body from the skinned shapes under its root, game/BoneMirrorRead.cpp, the log line "Body IK: the arm bones' partners for the mirror read from the body's skins"). The first-person right-arm bones, drawn turned over, land on the body's left arm with their cuff on its sleeve, and the body's hand gets the first-person hand's pose carried back the same way, a proper rotation of its own convention. If the skins do not give all twelve bones the arms are left to the body as before, with the why in the log. Unit flows: bone_mirror_test (a made-up symmetric bind with a turned left frame: the right-bone point drawn through the partner lies on the left bone's twin point in any pose, carried back it is the left arm's pose again), body_ik_test (the gate and the pairing).

The held object's hold point reads the pinned palm's axes, which are the real fingers' direction mirrored or not; the arrow, the bow grip and the two-hand handle compose from the mirrored nodes' world transforms and need nothing. The sheathed weapons on the body (section 4) are the body's, unmirrored.

Unit flows: bone_pin_test ("The left-handed mirror": the mirrored hand sits at the real controller with determinant -1 and is the right hand's pose on the mirrored controller carried through the midplane; a fit of it gives the right-handed calibration back), first_person_arms_logic_test (the root turned over between turn and base), backface_pass_test (the cull swap, the inside's cull), hand_mode_test (the rest mirrored only with the roles swapped), hand_input_test (the physical side).

## 3. Features

| Feature | Where | Left-handed today | Status |
| --- | --- | --- | --- |
| Attack, swing, power attacks, bow draw, reach back for an arrow | HandMode.cpp:232-308 | The weapon hand's, so the left controller. A power attack's direction (`ClassifyPowerSwing`, HandInput.h:276) is judged in the world: up, down, across, thrust. It needs no mirror. | OK |
| Left fist (hand-to-hand) | HandMode.cpp:310-341 | The other hand's, so the right controller. | OK |
| Block, shield | HandMode.cpp:223 | The other hand, the game's left arm, so the right controller. | OK |
| Casting (magicNode on the left palm) | CameraHook.cpp:4033-4035 | The game's left hand, so the right controller. | OK |
| Torch | "Bip01 L Hand" | The right controller. | OK |
| Two hands on a two-hander | TwoHandLogic.h, CameraHook.cpp | The other hand, the right controller, goes onto the handle. The weapon hand leads. | OK |
| Grab, throw, stow, shove, lead by hand | HandMode.cpp:501-511, CameraHook.cpp | By role, or either hand. | OK |
| The palm's direction for the item search | CameraHook.cpp (the two-hand item search) | `side` went by role but read the controller's own x: swapped, the palm pointed the wrong way. Now `vr::PalmSideFor(role, rolesSwapped)`: by the physical controller. | **Fixed 2026-10-20** (hand_input_test). |
| Laser yaw correction, the beam's origin offset | HandMode.cpp (the settings quad, the layer cursor, the poke), CameraHook.cpp (`LaserAnglesFor`, the hold point, the laser layer, the world pick ray) | The sign went by role, while the beam hangs on the physical controller: a left-hander's pointing beam was yawed twice the setting (10 degrees) the wrong way, its origin offset mirrored wrongly. Now `vr::LaserYawFor` and `vr::PhysicalLeftOf` (`LaserOffsetLocal`'s side) go by the physical controller, with `HandModeFrame::rolesSwapped` carrying the swap into the mode. | **Fixed 2026-10-20** (hand_input_test). |
| Laser device, crosshair hand | CameraHook.cpp:5578, 5414-5420 | Mapped back to the device. | OK |
| Wrist menu | HandMode.cpp:660-680, CameraHook.cpp:2765 | By role and x-symmetric: it goes on the weapon hand's wrist, the other hand points. | OK |
| Wrist HUD | HandMode.cpp:514, CameraHook.cpp:2768 | On the weapon hand's wrist. | OK |
| HUD on the hands | HandHud.h:95-96, CameraHook.cpp:1718-1719, 5438-5480 | Goes by **physical** controller. Bars and spell stay on the physical left, which is a left-hander's weapon hand. Weapon, effects and level-up stay on the physical right. | **Gap:** "left"/"right" should mean the other hand and the weapon hand under LeftHanded. Or add an auto/left/right per element, as the holsters have (section 4). |
| Walk direction | CameraHook.cpp:7146-7153, WalkDirection.h | The setting names physical controllers; the stick hand is known (`stickHandRight`). | OK |
| Sticks: walk, run, snap turn, sneak, jump, teleport | HandMode.cpp:404-440 | Swapped: walking on the right stick, turning and teleport on the left. | OK |
| Quick menu | HandInput.h:608, CameraHook.cpp:1205-1208, 1724 | Both paths by role, but on different hands. | Check in the headset |
| Inventory, menu B, escape | HandMode.cpp:400-431 | By role. | OK |
| **Weapon places (holsters)** | Holster.h, HolsterFit.h | Mirrored by x, and a side per place since 2026-09-30 (section 4). | **Built** |
| Stow at the body | Stow.h:68, 173 | The hand is by role; the spot's `centreRight` is not mirrored. | Minor gap |
| Activate | HandInput.h:593, 603 vs HandMode.cpp:471 | The swap does it. The `leftHanded` branch in `PlanHandControls` never runs (`in.leftHanded = false`). | Clean up the dead branch |
| Onboarding and settings texts | Onboarding.cpp:114, assets OBVR_Onboarding.xml:60, SettingsList.cpp:913 | Name physical buttons ("right stick up ... teleport", "Left B: inventory"). | **Gap:** wrong for a left-hander |
| SteamVR bindings | assets/input/*.json, OpenVRBackend.cpp:48 | Bound to physical hands; the swap comes after reading. | N/A |

## 4. Where each weapon hangs - built 2026-09-30

Every place has its own side, to mix as wanted:

| INI (`[Hands]`) | Settings row (Hands) | Values |
| --- | --- | --- |
| `HolsterOneHandSide` | One-hander side | `auto`, `right`, `left` |
| `HolsterTwoHandSide` | Two-hander side | `auto`, `right`, `left` |
| `HolsterBowSide` | Bow side | `auto`, `right`, `left` |

- **auto** is the old behaviour: a right-hander's places, mirrored with `LeftHanded=1`. A right-hander gets the one-hander at the left hip, the two-hander over the right shoulder and the bow behind the left shoulder. A left-hander gets everything the other way round.
- **right / left** put that place on that side of the body, whatever the handedness. The place's distance out to the side (`Holster*X`), its height and depth stay; only the side changes (`vr::ZoneFor`).
- The hand that draws does not change. One- and two-handers are drawn by the weapon hand, the bow by the other hand. A right-hander who sets the two-hander to `left` reaches across to the left shoulder with the right hand.
- **Fitting** ("Fit weapon places") keeps a chosen side where the hand really was (`vr::FittedSide`): set `right` and shown on the left, it becomes `left`, so a fit never puts a place on the other shoulder. `auto` stays `auto`, stored the right-handed way as before.
- Code: `vr/Holster.h` (`HolsterSide`, `ZoneFor`, `FittedSide`, covered by `holster_test` and `holster_fit_test`), `Config.cpp`, `SettingsList.cpp`, the fit's save in `CameraHook.cpp`.
- **Not tested in the game yet.** The game was running when it was built, so there was no harness run. The existing holster scenarios use the defaults (`auto`), which behave as before.

Not covered: the sheathed weapon's own model (the game's `SideWeapon` / `BackWeapon` nodes) still hangs where the game puts it, on the right-hander's side. It is seen in third person and in a mirror. Moving it with the side is a follow-up.

## 5. Order proposed

1. ~~The palm-direction bug and the laser-yaw inconsistency~~ - built 2026-10-20.
2. HUD on the hands by role (or auto/left/right per element, like the holsters): small, the elements' device choice is one table (HandHud.h).
3. ~~A separate hand calibration for left-handed play, and the mirrored rest offset of the arms~~ - the mirror makes one calibration serve both (2.1), the rest offset is mirrored with it.
4. The onboarding and settings texts naming the buttons by role: small, text only.
5. The sheathed weapon's model on the chosen side: small to medium, the body's nodes.
6. ~~Decide 2.1~~ - the mirror, built 2026-10-20, the body's arms across it the same day. Open from it: the headset's verdict on the mirrored picture (the cull, the cuff's seam).

## 6. Open: a full left-handed playthrough

The tester, 2026-09-30: "notiere irgendwo in den specs dass ich mal den ganzen mod als links händer durchteste". To do, by the tester in the headset: play the whole mod once with `[Hands] LeftHanded=1`, every feature in sections 2 to 4. Each finding goes into this spec as an open bug.
